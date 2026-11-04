/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PLAN90009: submit a zero length request descriptor.
 */
#include "lib/plan9_util.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    struct plan9_test_ctx ctx;
    plan9_test_init(&ctx, dev, vr);

    uint8_t *request = plan9_alloc(&ctx, 1);
    uint8_t *response = plan9_alloc(&ctx, PLAN9_RESPONSE_SIZE);
    if (!request || !response)
        TFAIL("test buffer exhausted");

    test_result_t result =
        plan9_transact(&ctx, request, 0, 0,
                       response, PLAN9_RESPONSE_SIZE, VRING_DESC_F_WRITE,
                       NULL);
    if (result != TEST_PASS)
        return result;

    return plan9_version(&ctx);
}

REGISTER_TEST(PLAN90009, VIRTIO_PCI_DEVICE_9P, test,
              "Handle a zero length request descriptor",
              VIRTIO_SPEC_V1_2, "-");
