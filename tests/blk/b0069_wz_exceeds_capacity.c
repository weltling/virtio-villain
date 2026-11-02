/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0069: Write zeroes spanning entire device capacity
 *
 * Spec 5.2.6.1: "A driver MUST NOT submit a request which would
 * cause a read or write beyond capacity."
 *
 * Submit WRITE_ZEROES with sector=0 and num_sectors=0xFFFFFFFF,
 * which almost certainly exceeds device capacity.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_wz_full_capacity(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_WRITE_ZEROES, 0);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    seg->sector = 0;
    seg->num_sectors = 0xFFFFFFFF; /* way beyond any 16MiB test disk */
    seg->flags = 0;

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, seg, sizeof(*seg), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0069, VIRTIO_PCI_DEVICE_BLK, test_blk_wz_full_capacity,
              "Write zeroes num_sectors=0xFFFFFFFF (exceeds capacity)",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
