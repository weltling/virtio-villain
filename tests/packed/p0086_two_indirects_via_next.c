/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0086: two_indirects_via_next
 *
 * Two packed descriptors, each marked INDIRECT, chained together with
 * the NEXT flag. The spec says an indirect descriptor must be the sole
 * descriptor of its chain, so it may not carry NEXT. The device must
 * reject the illegal chaining rather than following it.
 */
#include "tests/packed/packed_util.h"

static test_result_t test_packed_two_indirects_via_next(struct virtio_dev *dev,
                                                       struct vring_packed *vr)
{
    uint64_t t1 = pk_indirect_blk_table();
    uint64_t t2 = pk_indirect_blk_table();

    uint16_t a0 = vr->next_avail;
    uint8_t w0 = vr->wrap_counter;

    pk_append(vr, t1, 3 * sizeof(struct vring_packed_desc), a0,
              VRING_PACKED_DESC_F_INDIRECT | VRING_PACKED_DESC_F_NEXT);
    pk_append(vr, t2, 3 * sizeof(struct vring_packed_desc), a0,
              VRING_PACKED_DESC_F_INDIRECT);

    return vv_kick_and_wait_packed(dev, vr, 0, a0, w0, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0086, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_two_indirects_via_next,
                     "Packed two indirect descriptors chained via NEXT",
                     VIRTIO_SPEC_V1_2, "2.8.6");
