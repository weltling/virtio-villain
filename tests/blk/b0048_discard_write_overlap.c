/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0048: blk_discard_write_overlap
 *
 * Submit a DISCARD and a WRITE to the same sector range concurrently
 * (both in the avail ring before kicking). Tests device handling of
 * conflicting operations to overlapping regions in a single batch.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_discard_write_overlap(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    /* Request 1: DISCARD sector 0, 8 sectors */
    struct virtio_blk_outhdr *hdr1 = blk_hdr(VIRTIO_BLK_T_DISCARD, 0);
    struct virtio_blk_discard_write_zeroes *seg1 = vv_alloc_pages(1);
    uint8_t *status1 = vv_alloc_pages(1);

    seg1->sector = 0;
    seg1->num_sectors = 8;
    seg1->flags = 0;
    *status1 = 0xFF;

    /* Request 2: WRITE sector 0, 512 bytes */
    struct virtio_blk_outhdr *hdr2 = blk_hdr(VIRTIO_BLK_T_OUT, 0);
    uint8_t *wdata = vv_alloc_pages(1);
    uint8_t *status2 = vv_alloc_pages(1);

    memset(wdata, 0xAB, 512);
    *status2 = 0xFF;

    /* Chain 1: hdr1 -> seg1 -> status1 (descs 0-2) */
    vv_desc(vr, 0, hdr1, sizeof(*hdr1), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, seg1, sizeof(*seg1), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status1, 1, VRING_DESC_F_WRITE, 0);

    /* Chain 2: hdr2 -> wdata -> status2 (descs 3-5) */
    vv_desc(vr, 3, hdr2, sizeof(*hdr2), VRING_DESC_F_NEXT, 4);
    vv_desc(vr, 4, wdata, 512, VRING_DESC_F_NEXT, 5);
    vv_desc(vr, 5, status2, 1, VRING_DESC_F_WRITE, 0);

    /* Both in avail ring */
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 3);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0048, VIRTIO_PCI_DEVICE_BLK, test_blk_discard_write_overlap,
              "Concurrent DISCARD and WRITE to overlapping sectors",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
