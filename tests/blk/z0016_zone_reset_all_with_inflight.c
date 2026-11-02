/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0016: zone_reset_all_with_inflight
 *
 * Queue a ZONE_APPEND chain in slot 0 and a ZONE_RESET_ALL
 * chain in slot 1, place both in the avail ring before the kick.
 * Spec v1.3 5.2.6 requires the device to consume both entries
 * deterministically rather than dropping one or wedging.
 */
#include "lib/virtio_test.h"

static test_result_t test_zone_reset_all_inflight(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_blk_outhdr *h1 = blk_hdr(VIRTIO_BLK_T_ZONE_APPEND, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st1 = vv_alloc_pages(1);
    struct virtio_blk_outhdr *h2 = blk_hdr(VIRTIO_BLK_T_ZONE_RESET_ALL, 0);
    uint8_t *st2 = vv_alloc_pages(1);

    *st1 = 0xFF;

    *st2 = 0xFF;

    /* Chain 1 */
    vv_desc(vr, 0, h1, sizeof(*h1), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, st1, 1, VRING_DESC_F_WRITE, 0);

    /* Chain 2 */
    vv_desc(vr, 3, h2, sizeof(*h2), VRING_DESC_F_NEXT, 4);
    vv_desc(vr, 4, st2, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 3);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(Z0016, VIRTIO_PCI_DEVICE_BLK, test_zone_reset_all_inflight,
              "ZONE_RESET_ALL with a queued ZONE_APPEND",
              VIRTIO_SPEC_V1_3, "5.2.6");
