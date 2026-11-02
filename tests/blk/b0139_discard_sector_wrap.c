/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0139: Discard segment where sector + num_sectors wraps u64.
 *
 * Submit a DISCARD request with sector and num_sectors chosen so that
 * their sum overflows a 64-bit integer. The device must reject this.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_discard_sector_wrap(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_DISCARD, 0);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* sector + num_sectors > UINT64_MAX */
    seg->sector = UINT64_MAX - 10;
    seg->num_sectors = UINT32_MAX;
    seg->flags = 0;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, seg, sizeof(*seg), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0139, VIRTIO_PCI_DEVICE_BLK, test_blk_discard_sector_wrap,
              "Discard segment with sector+num_sectors u64 overflow",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
