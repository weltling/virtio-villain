/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0047: blk_write_zeroes_num_sectors_zero
 *
 * Submit a WRITE_ZEROES command with num_sectors set to 0. A zero-length
 * operation is a boundary case - the device should either complete
 * successfully (no-op) or reject it as invalid.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_wz_zero_sectors(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_WRITE_ZEROES, 0);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    seg->sector = 0;
    seg->num_sectors = 0; /* zero length! */
    seg->flags = 0;

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, seg, sizeof(*seg), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0047, VIRTIO_PCI_DEVICE_BLK, test_blk_wz_zero_sectors,
              "WRITE_ZEROES with num_sectors == 0",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
