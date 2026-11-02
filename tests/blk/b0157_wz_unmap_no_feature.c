/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0157: blk_wz_unmap_no_feature
 *
 * Submit a WRITE_ZEROES request with the unmap flag set but the
 * VIRTIO_BLK_F_DISCARD feature is not negotiated by the harness.
 * Spec 5.2.6.2 says unmap is only valid when DISCARD is negotiated.
 * The device must reject or stay silent without crashing.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_wz_unmap_no_feature(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_WRITE_ZEROES, 0);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    seg->sector = 0;
    seg->num_sectors = 8;
    seg->flags = VIRTIO_BLK_WRITE_ZEROES_FLAG_UNMAP;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, seg, sizeof(*seg), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0157, VIRTIO_PCI_DEVICE_BLK, test_blk_wz_unmap_no_feature,
              "WRITE_ZEROES unmap flag without DISCARD feature",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
