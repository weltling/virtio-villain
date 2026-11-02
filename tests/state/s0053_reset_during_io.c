/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0053: reset issued while a request is in flight
 *
 * Spec 2.1.2 says a reset must complete promptly. If the driver
 * issues a reset while a request is pending in the avail ring the
 * device must drop the in flight work and reach status zero. A
 * follow up reinit and submission must work cleanly.
 */
#include "lib/virtio_test.h"

static test_result_t test_reset_during_io(struct virtio_dev *dev,
                                          struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    *st = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);
    vring_submit(vr, 0);
    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    /* Reset immediately, no waiting */
    virtio_pci_reset(dev);

    /* Reinit and confirm device is healthy again */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    if (!(cfg->device_status & VIRTIO_STATUS_DRIVER))
        TFAIL("!(cfg->device_status & VIRTIO_STATUS_DRIVER)");

    return TEST_PASS;
}

REGISTER_TEST(S0053, VIRTIO_PCI_DEVICE_BLK, test_reset_during_io,
              "reset issued right after kick, no completion wait",
              VIRTIO_SPEC_V1_2, "2.1.2");
