/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0062: VIRTIO_BLK_T_GET_LIFETIME command (spec 5.2.6.3)
 *
 * Issue a GET_LIFETIME request. If the feature is not supported,
 * the device should reject or return an error status.
 */
#include "lib/virtio_test.h"

static test_result_t test_get_lifetime(struct virtio_dev *dev,
                                       struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_GET_LIFETIME, 0);
    struct virtio_blk_lifetime *life = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    *st = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, life, sizeof(*life), VRING_DESC_F_NEXT | VRING_DESC_F_WRITE,
            2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0062, VIRTIO_PCI_DEVICE_BLK, test_get_lifetime,
              "VIRTIO_BLK_T_GET_LIFETIME command",
              VIRTIO_SPEC_V1_2, "5.2.6.3");
