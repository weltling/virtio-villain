/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0070: requests complete while an MSI-X vector is masked
 *
 * Enable MSI-X and bind queue 0 to masked vector 0 before enabling
 * the queue and setting DRIVER_OK. Request a completion interrupt
 * but poll the used ring. This checks request progress, not interrupt
 * suppression or pending interrupt delivery after unmasking.
 */
#include "lib/virtio_test.h"
#include "lib/pci.h"

#include <errno.h>

#define MSIX_ENABLE        (1u << 31)
#define MSIX_FUNCTION_MASK (1u << 30)

static test_result_t run_masked_request(struct virtio_dev *dev, struct vring *vr,
                                        volatile struct msix_table_entry *t,
                                        int fd, uint8_t msix_pos)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint32_t control = pci_cfg_read32(fd, msix_pos);
    if (!(control & MSIX_ENABLE) || (control & MSIX_FUNCTION_MASK))
        TFAIL("MSI-X enable or function mask readback is incorrect");
    if (!(t[0].vector_ctrl & 1u))
        TFAIL("MSI-X vector mask was not retained");

    vring_prepare(dev, vr, 0);
    if (cfg->queue_enable != 0)
        TFAIL("queue 0 is enabled before MSI-X vector configuration");
    cfg->queue_msix_vector = 0;
    __sync_synchronize();
    uint16_t vector = cfg->queue_msix_vector;
    if (vector == VIRTIO_MSI_NO_VECTOR)
        TSKIP("device could not allocate MSI-X vector 0");
    if (vector != 0)
        TFAIL("queue MSI-X vector readback is 0x%04x, expected 0", vector);
    vr->avail->flags = 0;
    vr->avail->idx = 0;
    vr->used->idx = 0;
    __sync_synchronize();
    cfg->queue_enable = 1;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Submit a normal read */
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    *st = 0xFF;
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    virtio_pci_kick(dev, vr->queue);

    /* Poll used ring directly while vector is masked */
    int done = 0;
    for (int i = 0; i < VV_TIMEOUT_MS * vv_wait_scale; i++) {
        __sync_synchronize();
        if (vr->used->idx != 0) { done = 1; break; }
        usleep(1000);
    }

    if (!done)
        TWEDGED("!done");
    if (*st != 0)
        TFAIL("*st != 0");
    return TEST_PASS;
}

static test_result_t test_pci_msix_per_vec_mask(struct virtio_dev *dev,
                                                struct vring *vr)
{
    int fd = pci_cfg_open(dev->slot);
    if (fd < 0)
        TFAIL("open PCI config: %s", strerror(errno));
    uint8_t msix_pos = pci_find_capability(fd, PCI_CAP_ID_MSIX);
    if (!msix_pos) {
        close(fd);
        TSKIP("MSI-X capability unavailable");
    }
    uint32_t tobir = pci_cfg_read32(fd, msix_pos + 4);
    int bir = tobir & 0x7;
    uint32_t off = tobir & ~0x7u;
    if (bir > 5) {
        close(fd);
        TFAIL("MSI-X table uses invalid BAR %d", bir);
    }
    volatile void *bar = pci_map_bar(dev->slot, bir);
    if (!bar) {
        close(fd);
        TFAIL("cannot map MSI-X table BAR %d", bir);
    }
    volatile struct msix_table_entry *t =
        (volatile struct msix_table_entry *)((volatile uint8_t *)bar + off);
    uint32_t original = pci_cfg_read32(fd, msix_pos);
    if (virtio_pci_init_features(dev, 1ULL << VIRTIO_F_VERSION_1) < 0) {
        close(fd);
        TFAIL("device reset or feature negotiation failed");
    }

    /* Never unmask an active vector without an installed interrupt handler. */
    uint32_t original_mask = t[0].vector_ctrl;
    t[0].vector_ctrl = original_mask | 1u;
    __sync_synchronize();
    pci_cfg_write32(fd, msix_pos,
                    (original | MSIX_ENABLE) & ~MSIX_FUNCTION_MASK);
    test_result_t result = run_masked_request(dev, vr, t, fd, msix_pos);

    virtio_pci_reset(dev);
    pci_cfg_write32(fd, msix_pos, original & ~MSIX_ENABLE);
    __sync_synchronize();
    if (pci_cfg_read32(fd, msix_pos) & MSIX_ENABLE) {
        close(fd);
        TFAIL("MSI-X remained enabled after masked request");
    }
    t[0].vector_ctrl = original_mask;
    __sync_synchronize();
    close(fd);
    if (dev->common->device_status != 0)
        TWEDGED("device reset did not complete after masked request");
    return result;
}

REGISTER_TEST(PCI0070, VIRTIO_PCI_DEVICE_BLK, test_pci_msix_per_vec_mask,
              "request completes while MSI-X vector is masked",
              VIRTIO_SPEC_V1_2, "4.1.5.1.2");
