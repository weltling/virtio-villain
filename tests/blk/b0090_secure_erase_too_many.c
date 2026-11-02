/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0090: Secure erase segment count exceeds max_secure_erase_seg
 *
 * Spec 5.2.6.1: "VIRTIO_BLK_T_SECURE_ERASE requests MUST NOT
 * contain more than max_secure_erase_seg segments."
 *
 * Submit SECURE_ERASE with 256 segments (16*256=4096 bytes of data),
 * which should exceed any reasonable max_secure_erase_seg.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_secure_erase_too_many(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_SECURE_ERASE, 0);
    struct virtio_blk_discard_write_zeroes *segs = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    /* Fill 256 segments (4096 bytes = 1 page) */
    for (int i = 0; i < 256; i++) {
        segs[i].sector = i * 8;
        segs[i].num_sectors = 8;
        segs[i].flags = 0;
    }

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, segs, 256 * sizeof(segs[0]), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0090, VIRTIO_PCI_DEVICE_BLK, test_blk_secure_erase_too_many,
              "Secure erase with 256 segments (exceeds max_secure_erase_seg)",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
