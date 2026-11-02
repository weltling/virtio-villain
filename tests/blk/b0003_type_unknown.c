/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0003: blk_type_unknown
 *
 * Submit a request with an unknown type (0xDEADBEEF). The device should
 * return UNSUPP status, not crash.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_type_unknown(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(0xDEADBEEF, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0003, VIRTIO_PCI_DEVICE_BLK, test_blk_type_unknown,
              "Unknown request type (0xDEADBEEF)",
              VIRTIO_SPEC_V1_2, "5.2.6");
