/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0036: net_spurious_tx_kick
 *
 * Kick the TX queue without posting any descriptors. The device must
 * handle spurious notifications gracefully: check avail_idx, see
 * nothing new, and return to waiting without crashing.
 */
#include "tests/test.h"

static test_result_t test_net_spurious_tx_kick(struct virtio_dev *dev,
                                               struct vring *vr)
{
    /* avail_idx is 0 from harness setup - nothing posted */
    return vv_kick_expect_reject(dev, vr, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0036, VIRTIO_PCI_DEVICE_NET, test_net_spurious_tx_kick,
              "Spurious kick on TX queue with nothing posted",
              VIRTIO_SPEC_V1_2, "5.1.6.2");
