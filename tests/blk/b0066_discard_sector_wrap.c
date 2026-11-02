/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0066: Discard segment with sector+num_sectors wrapping u64
 *
 * Spec 5.2.6.1: "A driver MUST NOT submit a request which would
 * cause a read or write beyond capacity."
 *
 * Submit a DISCARD (opcode 11) with sector=0xFFFFFFFF00000000 and
 * num_sectors=0xFFFFFFFF so the sum wraps around u64. This exercises
 * the discard handler; the write zeroes path (opcode 13) is covered
 * by B0018.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_discard_sector_wrap(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_DISCARD, 0);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    seg->sector = 0xFFFFFFFF00000000ULL;
    seg->num_sectors = 0xFFFFFFFF;
    seg->flags = 0;

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, seg, sizeof(*seg), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0066, VIRTIO_PCI_DEVICE_BLK, test_blk_discard_sector_wrap,
              "Discard segment sector+num_sectors wraps u64",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
