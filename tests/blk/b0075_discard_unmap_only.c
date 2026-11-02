/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0075: Discard with unmap bit set (must be zero)
 *
 * Spec 5.2.6.1: "The unmap bit MUST be zero for discard commands."
 * Spec 5.2.6.2: "the device MUST set status to UNSUPP for discard
 * commands if the unmap flag is set."
 *
 * (Differs from b14 which tests unmap=1; this specifically sets
 * ONLY the unmap bit=1 with no other flags, on a clean sector range.)
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_discard_unmap_only(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_DISCARD, 0);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    seg->sector = 0;
    seg->num_sectors = 8;
    seg->flags = VIRTIO_BLK_WRITE_ZEROES_FLAG_UNMAP; /* must be zero for discard */

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, seg, sizeof(*seg), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0075, VIRTIO_PCI_DEVICE_BLK, test_blk_discard_unmap_only,
              "Discard with unmap=1 on valid sector range",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
