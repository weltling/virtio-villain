/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PLAN90007: mark the response descriptor device-readable.
 */
#include "lib/plan9_util.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    struct plan9_test_ctx ctx;
    plan9_test_init(&ctx, dev, vr);

    struct p9_version *request = plan9_build_version(&ctx);
    uint8_t *response = plan9_alloc(&ctx, PLAN9_RESPONSE_SIZE);
    if (!request || !response)
        TFAIL("test buffer exhausted");

    test_result_t result =
        plan9_transact(&ctx, request, sizeof(*request), 0,
                       response, PLAN9_RESPONSE_SIZE, 0, NULL);
    if (result != TEST_PASS)
        return result;

    return plan9_version(&ctx);
}

REGISTER_TEST(PLAN90007, VIRTIO_PCI_DEVICE_9P, test,
              "Handle a response descriptor without WRITE",
              VIRTIO_SPEC_V1_2, "-");
