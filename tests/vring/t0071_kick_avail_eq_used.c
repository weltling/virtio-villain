/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0071: kick_empty_avail_eq_used
 *
 * Kick when avail idx equals used idx (ring appears empty from both
 * perspectives). The device should detect nothing new is available
 * and not process stale descriptors from a previous iteration.
 */
#include "lib/virtio_test.h"

static test_result_t test_kick_empty_avail_eq_used(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* Set up a valid request in descriptor slots */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    /*
     * Set avail idx = 5, and pretend used has already consumed up to 5.
     * Then kick - the device should see nothing new.
     */
    vring_raw_set_avail_idx(vr, 5);
    vr->used->idx = 5;
    __sync_synchronize();

    /* Leave avail ring entries pointing at desc 0, but don't advance idx */
    return vv_kick_expect_reject(dev, vr, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0071, VIRTIO_PCI_DEVICE_BLK, test_kick_empty_avail_eq_used,
              "Kick with avail idx == used idx (empty ring confusion)",
              VIRTIO_SPEC_V1_2, "2.7.7");
