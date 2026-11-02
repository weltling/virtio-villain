/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0121: device reset unmaps all MSI-X events
 *
 * Virtio 1.2 section 4.1.5.1.2.1 requires queue and configuration
 * change events to be unmapped upon reset. Establish mappings to
 * vector 0, reset before and after DRIVER_OK and check NO_VECTOR.
 * Vector 0 remains masked to prevent delivery without an IRQ handler.
 */
#include "lib/virtio_test.h"
#include "lib/pci.h"

#include <errno.h>

#define MSIX_ENABLE        (1u << 31)
#define MSIX_FUNCTION_MASK (1u << 30)

static test_result_t check_reset(struct virtio_dev *dev, struct vring *vr,
                                 int fd, uint8_t cap, uint32_t control,
                                 volatile uint8_t *table, int active)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    const char *phase = active ? "after DRIVER_OK" : "before DRIVER_OK";
    pci_cfg_write32(fd, cap, control & ~MSIX_ENABLE);
    if (virtio_pci_init_features(dev, 1ULL << VIRTIO_F_VERSION_1) < 0)
        TFAIL("%s: device reset or feature negotiation failed", phase);
    vv_mmio_write32(table + 12, vv_mmio_read32(table + 12) | 1u);
    __sync_synchronize();
    if (!(vv_mmio_read32(table + 12) & 1u))
        TFAIL("%s: MSI-X vector 0 mask was not retained", phase);
    pci_cfg_write32(fd, cap, (control | MSIX_ENABLE) & ~MSIX_FUNCTION_MASK);
    if (!(pci_cfg_read32(fd, cap) & MSIX_ENABLE))
        TFAIL("%s: MSI-X enable was not retained", phase);

    uint16_t nq = cfg->num_queues;
    if (nq == 0)
        TFAIL("%s: device reports no queues", phase);
    vring_prepare(dev, vr, 0);
    if (cfg->queue_enable != 0)
        TFAIL("%s: queue 0 was not disabled by reset", phase);
    cfg->msix_config = 0;
    __sync_synchronize();
    uint16_t vector = cfg->msix_config;
    if (vector == VIRTIO_MSI_NO_VECTOR)
        TSKIP("%s: device could not allocate configuration vector 0", phase);
    if (vector != 0)
        TFAIL("%s: configuration vector readback is 0x%04x, expected 0",
              phase, vector);
    for (uint16_t q = 0; q < nq; q++) {
        cfg->queue_select = q;
        __sync_synchronize();
        cfg->queue_msix_vector = 0;
        __sync_synchronize();
        vector = cfg->queue_msix_vector;
        if (vector == VIRTIO_MSI_NO_VECTOR)
            TSKIP("%s: device could not allocate vector 0 for queue %u",
                  phase, q);
        if (vector != 0)
            TFAIL("%s: queue %u vector readback is 0x%04x, expected 0",
                  phase, q, vector);
    }
    cfg->queue_select = 0;
    __sync_synchronize();
    cfg->queue_enable = 1;
    __sync_synchronize();
    if (active) {
        cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
        __sync_synchronize();
        if (!(cfg->device_status & VIRTIO_STATUS_DRIVER_OK))
            TFAIL("%s: device did not retain DRIVER_OK", phase);
    }

    virtio_pci_reset(dev);
    if (cfg->device_status != 0)
        TWEDGED("%s: device reset did not complete", phase);
    vv_mmio_write32(table + 12, vv_mmio_read32(table + 12) | 1u);
    __sync_synchronize();
    pci_cfg_write32(fd, cap, (control | MSIX_ENABLE) & ~MSIX_FUNCTION_MASK);
    if (!(pci_cfg_read32(fd, cap) & MSIX_ENABLE))
        TFAIL("%s: MSI-X enable was not retained", phase);
    uint16_t config_vector = cfg->msix_config;
    nq = cfg->num_queues;
    if (nq == 0)
        TFAIL("%s: device reports no queues after reset", phase);
    unsigned bad_queues = 0;
    uint16_t first_bad_queue = 0;
    uint16_t first_bad_vector = VIRTIO_MSI_NO_VECTOR;
    for (uint16_t q = 0; q < nq; q++) {
        cfg->queue_select = q;
        __sync_synchronize();
        vector = cfg->queue_msix_vector;
        if (vector != VIRTIO_MSI_NO_VECTOR) {
            if (bad_queues == 0) {
                first_bad_queue = q;
                first_bad_vector = vector;
            }
            bad_queues++;
        }
    }
    if (bad_queues)
        TFAIL("%s reset: config vector=0x%04x, %u queue vectors not NO_VECTOR, "
              "first queue %u vector=0x%04x",
              phase, config_vector, bad_queues, first_bad_queue, first_bad_vector);
    if (config_vector != VIRTIO_MSI_NO_VECTOR)
        TFAIL("%s reset: config vector=0x%04x, expected 0xffff",
              phase, config_vector);
    return TEST_PASS;
}

static test_result_t test_pci_msix_reset_unmaps(struct virtio_dev *dev,
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
    uint32_t location = pci_cfg_read32(fd, cap + 4);
    unsigned bir = location & 7u;
    if (bir > 5) {
        close(fd);
        TFAIL("MSI-X table uses invalid BAR %u", bir);
    }
    volatile uint8_t *bar = pci_map_bar(dev->slot, bir);
    if (!bar) {
        close(fd);
        TFAIL("cannot map MSI-X table BAR %u", bir);
    }
    volatile uint8_t *table = bar + (location & ~7u);
    uint32_t original_mask = vv_mmio_read32(table + 12);
    vv_mmio_write32(table + 12, original_mask | 1u);
    __sync_synchronize();
    test_result_t result = TEST_PASS;
    for (int active = 0; active <= 1; active++) {
        test_result_t phase = check_reset(dev, vr, fd, cap, original, table, active);
        if (phase == TEST_FAIL) {
            result = TEST_FAIL;
        } else if (phase != TEST_PASS) {
            if (result != TEST_FAIL)
                result = phase;
            break;
        }
    }

    virtio_pci_reset(dev);
    pci_cfg_write32(fd, cap, original & ~MSIX_ENABLE);
    __sync_synchronize();
    if (pci_cfg_read32(fd, cap) & MSIX_ENABLE) {
        close(fd);
        TFAIL("MSI-X remained enabled after reset vector test");
    }
    vv_mmio_write32(table + 12, original_mask);
    __sync_synchronize();
    close(fd);
    if (dev->common->device_status != 0)
        TWEDGED("device reset did not complete after reset vector test");
    return result;
}

REGISTER_TEST_REQUIRES(PCI0121, VIRTIO_PCI_DEVICE_BLK, test_pci_msix_reset_unmaps,
                       "reset unmaps configuration and all queue MSI-X vectors",
                       VIRTIO_SPEC_V1_2, "4.1.5.1.2.1",
                       1ULL << VIRTIO_F_VERSION_1, 1);
