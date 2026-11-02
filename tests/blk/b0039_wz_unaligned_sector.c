/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0039: blk_write_zeroes_unaligned_sector
 *
 * Submit a WRITE_ZEROES with an odd sector number (7) that is likely
 * not aligned to the device's preferred block size. Tests whether the
 * VMM validates alignment constraints without crashing.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_wz_unaligned(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_WRITE_ZEROES, 0);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* Unaligned sector, odd count */
    seg->sector = 7;
    seg->num_sectors = 3;
    seg->flags = 0;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, seg, sizeof(*seg), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0039, VIRTIO_PCI_DEVICE_BLK, test_blk_wz_unaligned,
              "WRITE_ZEROES at unaligned sector offset",
              VIRTIO_SPEC_V1_2, "5.2.6.4");
