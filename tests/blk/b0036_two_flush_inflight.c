/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0036: blk_two_flush_inflight
 *
 * Submit two FLUSH requests simultaneously on the same queue.
 * Tests that the device correctly handles concurrent flush operations
 * without deadlock or double-completion.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_two_flush(struct virtio_dev *dev,
                                        struct vring *vr)
{
    struct virtio_blk_outhdr *hdr0 = blk_hdr(VIRTIO_BLK_T_FLUSH, 0);
    uint8_t *status0 = vv_alloc_pages(1);
    struct virtio_blk_outhdr *hdr1 = blk_hdr(VIRTIO_BLK_T_FLUSH, 0);
    uint8_t *status1 = vv_alloc_pages(1);

    *status0 = 0xFF;

    *status1 = 0xFF;

    /* Request 0: desc 0 (hdr) -> desc 1 (status) */
    vv_desc(vr, 0, hdr0, sizeof(*hdr0), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, status0, 1, VRING_DESC_F_WRITE, 0);

    /* Request 1: desc 2 (hdr) -> desc 3 (status) */
    vv_desc(vr, 2, hdr1, sizeof(*hdr1), VRING_DESC_F_NEXT, 3);
    vv_desc(vr, 3, status1, 1, VRING_DESC_F_WRITE, 0);

    /* Both in avail ring, single kick */
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0036, VIRTIO_PCI_DEVICE_BLK, test_blk_two_flush,
              "Two FLUSH requests in-flight simultaneously",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
