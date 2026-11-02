/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0033: double_driver_ok
 *
 * Set DRIVER_OK twice. This should be idempotent; the device must not
 * re-process initialization or crash.
 */
#include "lib/virtio_test.h"

static test_result_t test_double_driver_ok(struct virtio_dev *dev,
                                           struct vring *vr)
{
    /* DRIVER_OK was already set by the harness. Set it again. */
    dev->common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    usleep(10000);

    /* Set it a third time for good measure */
    dev->common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Now do a normal I/O to verify device still works */
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0033, VIRTIO_PCI_DEVICE_BLK, test_double_driver_ok,
              "Set DRIVER_OK twice",
              VIRTIO_SPEC_V1_2, "3.1");
