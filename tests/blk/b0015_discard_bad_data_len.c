/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0015: blk_discard_bad_data_len
 *
 * Submit a DISCARD request where the data length is not a multiple of
 * sizeof(struct virtio_blk_discard_write_zeroes) (16 bytes).
 * The device must reject the malformed payload.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_discard_bad_len(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_DISCARD, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* Fill with something resembling a segment */

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    /* 17 bytes: not a multiple of 16 */
    vv_desc(vr, 1, data, 17, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0015, VIRTIO_PCI_DEVICE_BLK, test_blk_discard_bad_len,
              "DISCARD with data length not multiple of 16",
              VIRTIO_SPEC_V1_2, "5.2.6");
