/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0095: Write zeroes with unmap flag but no DISCARD feature
 *
 * Spec 5.2.6.1: The unmap flag in write zeroes segments requires
 * VIRTIO_BLK_F_DISCARD. Set unmap without that feature.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_wz_unmap_no_discard(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_WRITE_ZEROES, 0);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    seg->sector = 0;
    seg->num_sectors = 8;
    seg->flags = VIRTIO_BLK_WRITE_ZEROES_FLAG_UNMAP;

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, seg, sizeof(*seg), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0095, VIRTIO_PCI_DEVICE_BLK, test_blk_wz_unmap_no_discard,
              "Write zeroes with unmap flag but no DISCARD feature",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
