/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0163: blk_discard_then_read_same
 *
 * Submit a DISCARD over sectors 0..7, then immediately submit a
 * READ of sector 0 in the same avail ring batch. Spec 5.2.6.2
 * says DISCARD MAY deallocate the range; the device must not
 * crash or return stale UAF backing when a read races immediately
 * behind the unmap. Result data is not asserted because spec does
 * not mandate zero return after DISCARD.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_discard_then_read_same(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_BLK_F_DISCARD))
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr1 = blk_hdr(VIRTIO_BLK_T_DISCARD, 0);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status1 = vv_alloc_pages(1);

    struct virtio_blk_outhdr *hdr2 = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status2 = vv_alloc_pages(1);

    seg->sector = 0;
    seg->num_sectors = 8;
    seg->flags = 0;
    *status1 = 0xFF;

    memset(data, 0xAA, 4096);
    *status2 = 0xFF;

    /* Chain 1: DISCARD (descs 0,1,2) */
    vv_desc(vr, 0, hdr1, sizeof(*hdr1), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, seg, sizeof(*seg), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status1, 1, VRING_DESC_F_WRITE, 0);

    /* Chain 2: READ sector 0, 4KB (descs 3,4,5) */
    vv_desc(vr, 3, hdr2, sizeof(*hdr2), VRING_DESC_F_NEXT, 4);
    vv_desc(vr, 4, data, 4096, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 5);
    vv_desc(vr, 5, status2, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 3);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(B0163, VIRTIO_PCI_DEVICE_BLK, test_blk_discard_then_read_same,
              "Discard sector range then read same sector in one batch",
              VIRTIO_SPEC_V1_2, "5.2.6.2",
              (1ULL << VIRTIO_BLK_F_DISCARD), 0);
