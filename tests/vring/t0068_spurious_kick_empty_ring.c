/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0068: spurious_kick_empty_ring
 *
 * Kick the queue without posting any descriptors. The device must
 * handle spurious notifications gracefully: check avail_idx, see
 * nothing new, and return to waiting without crashing or advancing
 * the used ring.
 */
#include "tests/test.h"

static test_result_t test_spurious_kick(struct virtio_dev *dev,
                                        struct vring *vr)
{
    /* avail_idx is 0 from harness setup - nothing posted */
    return vv_kick_expect_reject(dev, vr, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0068, VIRTIO_PCI_DEVICE_BLK, test_spurious_kick,
              "Spurious kick with empty avail ring",
              VIRTIO_SPEC_V1_2, "2.7.13");
