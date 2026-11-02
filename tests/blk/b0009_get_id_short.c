/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0009: blk_get_id_short_buffer
 *
 * Submit a GET_ID request with a data buffer shorter than 20 bytes
 * (the spec defines VIRTIO_BLK_ID_BYTES = 20).
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_get_id_short(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_GET_ID, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* GET_ID with only 4 bytes for data (needs 20) */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 4, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0009, VIRTIO_PCI_DEVICE_BLK, test_blk_get_id_short,
              "GET_ID with buffer < 20 bytes",
              VIRTIO_SPEC_V1_2, "5.2.6");
