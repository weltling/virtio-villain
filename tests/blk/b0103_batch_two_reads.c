/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0103: Multiple requests in a single kick (batch submission)
 *
 * Submit two independent READ requests at once by placing two
 * descriptor chains in the avail ring before a single kick.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_batch_two_reads(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *hdr1 = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data1 = vv_alloc_pages(1);
    uint8_t *status1 = vv_alloc_pages(1);

    struct virtio_blk_outhdr *hdr2 = blk_hdr(VIRTIO_BLK_T_IN, 1);
    uint8_t *data2 = vv_alloc_pages(1);
    uint8_t *status2 = vv_alloc_pages(1);

    *status1 = 0xFF;

    *status2 = 0xFF;

    /* Chain 1: descs 0-1-2 */
    vv_desc(vr, 0, hdr1, sizeof(*hdr1), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data1, 512, VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status1, 1, VRING_DESC_F_WRITE, 0);

    /* Chain 2: descs 3-4-5 */
    vv_desc(vr, 3, hdr2, sizeof(*hdr2), VRING_DESC_F_NEXT, 4);
    vv_desc(vr, 4, data2, 512, VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 5);
    vv_desc(vr, 5, status2, 1, VRING_DESC_F_WRITE, 0);

    /* Both chains in avail ring */
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 3);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0103, VIRTIO_PCI_DEVICE_BLK, test_blk_batch_two_reads,
              "Batch: two READ requests submitted with single kick",
              VIRTIO_SPEC_V1_2, "5.2.6");
