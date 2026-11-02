/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0006: blk_data_not_512_multiple
 *
 * Submit a read request with data length = 513 (not a multiple of 512).
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_data_not_512(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    /* 513 bytes - not a sector multiple */
    vv_desc(vr, 1, data, 513, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0006, VIRTIO_PCI_DEVICE_BLK, test_blk_data_not_512,
              "Data length not a multiple of 512",
              VIRTIO_SPEC_V1_2, "5.2.6");
