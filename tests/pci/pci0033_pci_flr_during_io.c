/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0033: Trigger PCI Function Level Reset (write device_status=0)
 * while I/O is in flight.
 *
 * Spec 4.1.4.3.1: Writing 0 to device_status triggers a device
 * reset. The device must cleanly handle in-flight requests and
 * return to a usable state after re-initialization.
 */
#include "lib/virtio_test.h"

static test_result_t test_pci_flr_during_io(struct virtio_dev *dev,
                                            struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Submit a read request */
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    *st = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);

    vring_submit(vr, 0);

    /* Kick then immediately reset (FLR equivalent) */
    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    /* Immediate FLR: write 0 to device_status */
    cfg->device_status = 0;
    __sync_synchronize();

    /* Wait for reset to take effect */
    int tries = 200;
    while (tries-- > 0 && cfg->device_status != 0)
        usleep(1000);

    if (cfg->device_status != 0)
        TWEDGED("cfg->device_status != 0");

    /* Re-initialize the device to verify recovery */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = virtio_pci_base_features_hi(dev);
    __sync_synchronize();

    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(5000);

    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        TREJECT("!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK)");

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    return TEST_PASS;
}

REGISTER_TEST(PCI0033, VIRTIO_PCI_DEVICE_BLK, test_pci_flr_during_io,
              "PCI FLR (device_status=0) during in-flight I/O",
              VIRTIO_SPEC_V1_2, "4.1.4.3.1");
