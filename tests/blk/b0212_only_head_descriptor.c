/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0212: blk_only_head_descriptor
 *
 * Submit a request that consists of the header descriptor alone: a
 * single readable 16 byte descriptor with no VRING_DESC_F_NEXT, so the
 * chain carries no data descriptor and no status descriptor. A virtio
 * blk request always needs at least the status byte descriptor, so the
 * device must decline the request without crashing or wedging.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_only_head_descriptor(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0x7fffffff);

    /* Lone header descriptor: readable, no NEXT, no data, no status. */
    vv_desc(vr, 0, hdr, sizeof(*hdr), 0, 0);

    vring_submit(vr, 0);

    return vv_kick_expect_reject(dev, vr, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0212, VIRTIO_PCI_DEVICE_BLK, test_blk_only_head_descriptor,
              "Request with only the head descriptor and no status",
              VIRTIO_SPEC_V1_2, "5.2.6");
