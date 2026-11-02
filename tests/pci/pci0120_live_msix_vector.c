/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0120: live queue MSI-X unmapping and remapping
 *
 * Virtio 1.2 section 4.1.5.1.2.1 requires event mapping and unmapping.
 * After DRIVER_OK, unmap queue 0 and complete a request without setting
 * its old pending bit. Remap to vector 1 and verify its pending bit.
 * Both vectors remain masked so no interrupt handler is needed.
 */
#include "lib/virtio_test.h"
#include "lib/pci.h"

#include <errno.h>

#define MSIX_ENABLE        (1u << 31)
#define MSIX_FUNCTION_MASK (1u << 30)

static volatile uint8_t *map_msix_region(struct virtio_dev *dev, uint32_t location)
{
    unsigned bir = location & 7u;
    if (bir > 5)
        return NULL;
    volatile uint8_t *bar = pci_map_bar(dev->slot, bir);
    return bar ? bar + (location & ~7u) : NULL;
}

static test_result_t submit_read(struct virtio_dev *dev, struct vring *vr,
                                 uint16_t expected_idx, const char *phase)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);
    *status = 0xff;
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);
    vring_submit(vr, 0);
    virtio_pci_kick(dev, vr->queue);
    int scale = vv_wait_scale > 0 ? vv_wait_scale : 1;
    int done = 0;
    for (int i = 0; i < VV_TIMEOUT_MS * scale; i++) {
        __sync_synchronize();
        if (vr->used->idx != expected_idx - 1) {
            done = 1;
            break;
        }
        usleep(1000);
    }
    if (!done)
        TFAIL("%s request did not complete", phase);
    if (vr->used->idx != expected_idx ||
        vr->used->ring[(expected_idx - 1) % vr->size].id != 0)
        TFAIL("%s request produced an unexpected used ring entry", phase);
    if (*status != 0)
        TFAIL("%s request returned block status 0x%02x", phase, *status);
    return TEST_PASS;
}

static test_result_t check_live_vectors(struct virtio_dev *dev, struct vring *vr,
                                        volatile uint8_t *table,
                                        volatile uint8_t *pba,
                                        int fd, uint8_t cap)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint32_t control = pci_cfg_read32(fd, cap);
    if (!(control & MSIX_ENABLE) || (control & MSIX_FUNCTION_MASK))
        TFAIL("MSI-X enable or function mask readback is incorrect");
    for (unsigned i = 0; i < 2; i++) {
        if (!(vv_mmio_read32(table + i * 16 + 12) & 1u))
            TFAIL("MSI-X vector %u is not masked", i);
    }

    vring_prepare(dev, vr, 0);
    if (cfg->queue_enable != 0)
        TFAIL("queue 0 was not disabled by reset");
    cfg->msix_config = VIRTIO_MSI_NO_VECTOR;
    cfg->queue_msix_vector = 0;
    __sync_synchronize();
    uint16_t vector = cfg->queue_msix_vector;
    if (vector == VIRTIO_MSI_NO_VECTOR)
        TSKIP("device could not allocate initial MSI-X vector 0");
    if (vector != 0)
        TFAIL("initial queue vector is 0x%04x, expected 0", vector);
    if (cfg->msix_config != VIRTIO_MSI_NO_VECTOR)
        TFAIL("configuration change interrupts could not be unmapped");
    vr->avail->flags = 0;
    vr->avail->idx = 0;
    vr->used->idx = 0;
    __sync_synchronize();
    cfg->queue_enable = 1;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    if (!(cfg->device_status & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("device did not retain DRIVER_OK");

    cfg->queue_msix_vector = VIRTIO_MSI_NO_VECTOR;
    __sync_synchronize();
    vector = cfg->queue_msix_vector;
    if (vector != VIRTIO_MSI_NO_VECTOR)
        TFAIL("live queue unmap readback is 0x%04x, expected 0xffff", vector);
    if (vv_mmio_read32(pba) & 3u)
        TFAIL("MSI-X vectors are already pending before I/O");
    test_result_t result = submit_read(dev, vr, 1, "unmapped");
    if (result != TEST_PASS)
        return result;
    int scale = vv_wait_scale > 0 ? vv_wait_scale : 1;
    for (int i = 0; i < VV_TIMEOUT_MS * scale; i++) {
        __sync_synchronize();
        if (vv_mmio_read32(pba) & 3u)
            TFAIL("unmapped queue request set an MSI-X pending bit");
        usleep(1000);
    }

    cfg->queue_msix_vector = 1;
    __sync_synchronize();
    vector = cfg->queue_msix_vector;
    if (vector == VIRTIO_MSI_NO_VECTOR)
        TSKIP("device could not allocate live MSI-X vector 1");
    if (vector != 1)
        TFAIL("live queue remap readback is 0x%04x, expected 1", vector);
    result = submit_read(dev, vr, 2, "remapped");
    if (result != TEST_PASS)
        return result;
    for (int i = 0; i < VV_TIMEOUT_MS * scale; i++) {
        __sync_synchronize();
        uint32_t pending = vv_mmio_read32(pba);
        if (pending & 1u)
            TFAIL("request interrupt still routed to old MSI-X vector 0");
        if (pending & 2u)
            return TEST_PASS;
        usleep(1000);
    }
    TFAIL("remapped request did not set pending bit for MSI-X vector 1");
}

static test_result_t test_pci_live_msix_vector(struct virtio_dev *dev,
                                               struct vring *vr)
{
    int fd = pci_cfg_open(dev->slot);
    if (fd < 0)
        TFAIL("open PCI config: %s", strerror(errno));
    uint8_t cap = pci_find_capability(fd, PCI_CAP_ID_MSIX);
    if (!cap) {
        close(fd);
        TSKIP("MSI-X capability unavailable");
    }
    uint32_t original = pci_cfg_read32(fd, cap);
    unsigned vectors = ((original >> 16) & 0x7ffu) + 1;
    if (vectors < 2) {
        close(fd);
        TSKIP("test requires two MSI-X vectors");
    }
    volatile uint8_t *table =
        map_msix_region(dev, pci_cfg_read32(fd, cap + 4));
    volatile uint8_t *pba =
        map_msix_region(dev, pci_cfg_read32(fd, cap + 8));
    if (!table || !pba) {
        close(fd);
        TFAIL("cannot map MSI-X table or pending bit array");
    }
    if (virtio_pci_init_features(dev, 1ULL << VIRTIO_F_VERSION_1) < 0) {
        close(fd);
        TFAIL("device reset or feature negotiation failed");
    }
    uint32_t original_masks[2];
    for (unsigned i = 0; i < 2; i++) {
        original_masks[i] = vv_mmio_read32(table + i * 16 + 12);
        vv_mmio_write32(table + i * 16 + 12, original_masks[i] | 1u);
    }
    __sync_synchronize();
    pci_cfg_write32(fd, cap, (original | MSIX_ENABLE) & ~MSIX_FUNCTION_MASK);
    test_result_t result = check_live_vectors(dev, vr, table, pba, fd, cap);

    virtio_pci_reset(dev);
    pci_cfg_write32(fd, cap, original & ~MSIX_ENABLE);
    __sync_synchronize();
    if (pci_cfg_read32(fd, cap) & MSIX_ENABLE) {
        close(fd);
        TFAIL("MSI-X remained enabled after live vector test");
    }
    for (unsigned i = 0; i < 2; i++)
        vv_mmio_write32(table + i * 16 + 12, original_masks[i]);
    __sync_synchronize();
    close(fd);
    if (dev->common->device_status != 0)
        TWEDGED("device reset did not complete after live vector test");
    return result;
}

REGISTER_TEST_REQUIRES(PCI0120, VIRTIO_PCI_DEVICE_BLK, test_pci_live_msix_vector,
                       "live queue MSI-X unmapping and interrupt remapping",
                       VIRTIO_SPEC_V1_2, "4.1.5.1.2.1",
                       1ULL << VIRTIO_F_VERSION_1, 1);
