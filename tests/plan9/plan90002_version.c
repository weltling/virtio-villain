/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PLAN90002: negotiate 9P2000.L with a Tversion request.
 *
 * A valid Rversion must preserve NOTAG, report a bounded message size, and
 * select the requested protocol version.
 */
#include "lib/plan9_util.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    struct plan9_test_ctx ctx;
    plan9_test_init(&ctx, dev, vr);
    return plan9_version(&ctx);
}

REGISTER_TEST(PLAN90002, VIRTIO_PCI_DEVICE_9P, test,
              "Negotiate 9P2000.L with Tversion",
              VIRTIO_SPEC_V1_2, "-");
