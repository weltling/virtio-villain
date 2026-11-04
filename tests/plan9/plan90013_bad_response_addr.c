/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PLAN90013: use a writable response address outside guest memory.
 */
#include "lib/plan9_util.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    struct plan9_test_ctx ctx;
    plan9_test_init(&ctx, dev, vr);

    struct p9_version *request = plan9_build_version(&ctx);
    if (!request)
        TFAIL("test buffer exhausted");

    struct plan9_chain_desc descs[] = {
        {
            .addr = vv_virt_to_phys(request),
            .len = sizeof(*request),
            .flags = 0,
        },
        {
            .addr = 0xffffffffffff0000ULL,
            .len = PLAN9_RESPONSE_SIZE,
            .flags = VRING_DESC_F_WRITE,
        },
    };
    test_result_t result =
        plan9_submit_chain(&ctx, descs, 2, PLAN9_RESPONSE_SIZE, NULL);
    if (result != TEST_PASS)
        return result;

    return plan9_version(&ctx);
}

REGISTER_TEST(PLAN90013, VIRTIO_PCI_DEVICE_9P, test,
              "Handle a response address outside guest memory",
              VIRTIO_SPEC_V1_2, "-");
