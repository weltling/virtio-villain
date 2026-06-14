/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0006: config_read_during_negotiation
 *
 * Access device configuration space fields that are gated behind features
 * BEFORE setting FEATURES_OK. The spec says the driver MUST check that
 * the corresponding feature is offered before accessing that part of
 * the configuration space (3.1.1).
 *
 * Some VMMs expose config space unconditionally. A well-behaved VMM
 * should either return zeros/defaults or fault on accesses to
 * feature-gated config fields prior to feature acceptance.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"
#include "lib/pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_config_read_during_negotiation(struct virtio_dev *dev,
                                                         struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)vr;

    /* Reset and go only to DRIVER state (before features) */
    virtio_pci_reset(dev);

    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /*
     * Now read config space fields that require features not yet accepted.
     * For virtio-blk, the capacity field (offset 0) is always present,
     * but fields like writeback (offset 32, requires CONFIG_WCE),
     * max_discard_sectors (requires DISCARD), etc. are gated.
     *
     * We'll read 64 bytes of device config space via the BAR, past the
     * common config structure. This exercises whether the VMM guards
     * these reads.
     *
     * We find the device-specific config by scanning PCI caps for
     * VIRTIO_PCI_CAP_DEVICE_CFG.
     */
    int fd = pci_cfg_open(dev->slot);
    if (fd < 0)
        return TEST_SKIP;

    uint8_t cap_ptr = pci_cfg_read8(fd, 0x34);
    uint32_t dev_cfg_offset = 0;
    uint8_t dev_cfg_bar = 0;

    while (cap_ptr) {
        uint8_t cap_id = pci_cfg_read8(fd, cap_ptr);
        uint8_t cap_next = pci_cfg_read8(fd, cap_ptr + 1);
        if (cap_id == 0x09) { /* Vendor-specific */
            uint8_t cfg_type = pci_cfg_read8(fd, cap_ptr + 3);
            if (cfg_type == VIRTIO_PCI_CAP_DEVICE_CFG) {
                dev_cfg_bar = pci_cfg_read8(fd, cap_ptr + 4);
                dev_cfg_offset = pci_cfg_read32(fd, cap_ptr + 8);
                break;
            }
        }
        cap_ptr = cap_next;
    }
    close(fd);

    if (!dev_cfg_offset)
        return TEST_SKIP;

    (void)dev_cfg_bar;

    /* Read device config fields while still in DRIVER state */
    volatile uint8_t *dev_cfg = (volatile uint8_t *)dev->bar + dev_cfg_offset;

    /* Read capacity (always valid, 8 bytes at offset 0) */
    volatile uint64_t *cap_ptr_field = (volatile uint64_t *)dev_cfg;
    uint64_t capacity = *cap_ptr_field;
    (void)capacity;

    /* Read beyond capacity into feature-gated fields */
    for (int i = 8; i < 64; i += 4) {
        volatile uint32_t *p = (volatile uint32_t *)(dev_cfg + i);
        uint32_t val = *p;
        (void)val;
    }

    usleep(50000);

    /*
     * Now complete init properly and verify device still works.
     */
    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = 0;
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        return TEST_SKIP;

    struct vring vr2;
    vring_alloc(&vr2, 16);
    vring_attach(dev, &vr2, 0);
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Quick I/O to confirm device is alive */
    uint8_t *buf = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);
    struct {
        uint32_t type;
        uint32_t ioprio;
        uint64_t sector;
    } __attribute__((packed)) *hdr = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(&vr2, 0, vv_virt_to_phys(hdr), 16,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&vr2, 1, vv_virt_to_phys(buf), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&vr2, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&vr2, 0, 0);
    vring_raw_set_avail_idx(&vr2, 1);

    return vv_kick_and_wait(dev, &vr2, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0006, VIRTIO_PCI_DEVICE_BLK, test_config_read_during_negotiation,
              "Read feature-gated config fields before FEATURES_OK",
              VIRTIO_SPEC_V1_2, "3.1.1");
