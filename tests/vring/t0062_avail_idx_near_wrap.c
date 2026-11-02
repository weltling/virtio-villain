/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0062: avail_idx_near_wrap
 *
 * Set avail idx near UINT16_MAX to exercise wrap-around behavior.
 * Submit a request at the wrap point. A VMM that uses signed arithmetic
 * or doesn't handle the 16-bit wrap correctly may reject the request.
 */
#include "lib/virtio_test.h"

static test_result_t test_avail_idx_near_wrap(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    /*
     * Set avail idx to 0xFFFE, put entry at position (0xFFFE % queue_size),
     * then bump to 0xFFFF - this exercises the 16-bit wrap boundary.
     */
    uint16_t base_idx = 0xFFFE;
    uint16_t slot = base_idx % vr->size;

    vring_raw_set_avail(vr, slot, 0);
    vring_raw_set_avail_idx(vr, base_idx + 1);

    /* Pretend we've gone around many times - set used idx to match */
    vr->used->idx = base_idx;
    __sync_synchronize();

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0062, VIRTIO_PCI_DEVICE_BLK, test_avail_idx_near_wrap,
              "Available ring idx near UINT16_MAX wrap boundary",
              VIRTIO_SPEC_V1_2, "2.7.6");
