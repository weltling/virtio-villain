/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0012: blk_status_too_short
 *
 * Submit a request where the status descriptor has len = 0. The device
 * needs at least 1 byte to write the completion status.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_status_too_short(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    /* Status descriptor with len = 0 */
    vv_desc(vr, 2, status, 0, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0012, VIRTIO_PCI_DEVICE_BLK, test_blk_status_too_short,
              "Status descriptor with len = 0",
              VIRTIO_SPEC_V1_2, "5.2.6");
