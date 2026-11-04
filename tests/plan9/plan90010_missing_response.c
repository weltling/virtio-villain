/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PLAN90010: submit Tversion without a writable response descriptor.
 */
#include "lib/plan9_util.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    struct plan9_test_ctx ctx;
    plan9_test_init(&ctx, dev, vr);

    struct p9_version *request = plan9_build_version(&ctx);
    if (!request)
        TFAIL("test buffer exhausted");

    struct plan9_chain_desc desc = {
        .addr = vv_virt_to_phys(request),
        .len = sizeof(*request),
        .flags = 0,
    };
    test_result_t result =
        plan9_submit_chain(&ctx, &desc, 1, 0, NULL);
    if (result != TEST_PASS)
        return result;

    return plan9_version(&ctx);
}

REGISTER_TEST(PLAN90010, VIRTIO_PCI_DEVICE_9P, test,
              "Handle Tversion without a response descriptor",
              VIRTIO_SPEC_V1_2, "-");
