/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PLAN90006: provide too little writable space for Rversion.
 */
#include "lib/plan9_util.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    struct plan9_test_ctx ctx;
    plan9_test_init(&ctx, dev, vr);

    struct p9_version *request = plan9_build_version(&ctx);
    uint8_t *response = plan9_alloc(&ctx, sizeof(struct p9_header));
    if (!request || !response)
        TFAIL("test buffer exhausted");

    test_result_t result =
        plan9_transact(&ctx, request, sizeof(*request), 0,
                       response, sizeof(struct p9_header),
                       VRING_DESC_F_WRITE, NULL);
    if (result != TEST_PASS)
        return result;

    return plan9_version(&ctx);
}

REGISTER_TEST(PLAN90006, VIRTIO_PCI_DEVICE_9P, test,
              "Handle a response buffer smaller than Rversion",
              VIRTIO_SPEC_V1_2, "-");
