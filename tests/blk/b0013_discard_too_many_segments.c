/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0013: blk_discard_too_many_segments
 *
 * Submit a DISCARD request with more segments than max_discard_seg.
 * The device must reject the oversized request.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_discard_too_many(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_DISCARD, 0);
    /* Pack 256 segments - way beyond any reasonable max_discard_seg */
    struct virtio_blk_discard_write_zeroes *segs = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* Fill 256 discard segments */
    for (int i = 0; i < 256; i++) {
        segs[i].sector = (uint64_t)i * 8;
        segs[i].num_sectors = 8;
        segs[i].flags = 0;
    }

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, segs, 256 * sizeof(struct virtio_blk_discard_write_zeroes),
            VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0013, VIRTIO_PCI_DEVICE_BLK, test_blk_discard_too_many,
              "DISCARD with too many segments",
              VIRTIO_SPEC_V1_2, "5.2.6");
