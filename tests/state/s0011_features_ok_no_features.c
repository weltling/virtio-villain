/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0011: features_ok_no_features
 *
 * Write FEATURES_OK without having written any driver_features first.
 * While technically valid (negotiating zero features), this tests the
 * edge case where FEATURES_OK is set before driver_feature_select/
 * driver_feature have been touched at all - the driver_feature register
 * may contain stale PCI bus values.
 *
 * Then proceed to set DRIVER_OK and do I/O, verifying the VMM doesn't
 * assume any features are active based on uninitialized register state.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_features_ok_no_features(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)vr;

    /* Reset */
    virtio_pci_reset(dev);

    /* ACKNOWLEDGE + DRIVER */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /*
     * Skip writing driver_feature_select and driver_feature entirely.
     * Go straight to FEATURES_OK.
     */
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK)) {
        /* Device rejected - that's fine behavior */
        TREJECT("!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    /* Set up queue and DRIVER_OK */
    struct vring vr2;
    vring_alloc(&vr2, 16);
    vring_attach(dev, &vr2, 0);
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Try a simple read request */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(&vr2, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&vr2, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&vr2, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&vr2, 0, 0);
    vring_raw_set_avail_idx(&vr2, 1);

    return vv_kick_and_wait(dev, &vr2, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0011, VIRTIO_PCI_DEVICE_BLK, test_features_ok_no_features,
              "Set FEATURES_OK without writing driver_feature registers",
              VIRTIO_SPEC_V1_2, "3.1.1");
