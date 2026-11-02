/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0127: GET_LIFETIME with undersized response buffer.
 *
 * Submit a GET_LIFETIME request with a data buffer smaller than the
 * expected lifetime response structure.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_get_lifetime_short(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_GET_LIFETIME, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* Response needs at least 8 bytes, give only 2 */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 2, VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0127, VIRTIO_PCI_DEVICE_BLK, test_blk_get_lifetime_short,
              "GET_LIFETIME with undersized response buffer",
              VIRTIO_SPEC_V1_2, "5.2.6.3");
