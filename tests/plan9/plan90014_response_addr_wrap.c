/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PLAN90014: make the writable response range wrap the GPA space.
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
            .addr = 0xfffffffffffff000ULL,
            .len = 0x2000,
            .flags = VRING_DESC_F_WRITE,
        },
    };
    test_result_t result =
        plan9_submit_chain(&ctx, descs, 2, 0x2000, NULL);
    if (result != TEST_PASS)
        return result;

    return plan9_version(&ctx);
}

REGISTER_TEST(PLAN90014, VIRTIO_PCI_DEVICE_9P, test,
              "Handle a response range that wraps 64 bits",
              VIRTIO_SPEC_V1_2, "-");
