/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PLAN90011: split a valid Tversion across readable descriptors.
 */
#include "lib/plan9_util.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    struct plan9_test_ctx ctx;
    plan9_test_init(&ctx, dev, vr);

    struct p9_version *request = plan9_build_version(&ctx);
    struct p9_version *response =
        plan9_alloc(&ctx, PLAN9_RESPONSE_SIZE);
    if (!request || !response)
        TFAIL("test buffer exhausted");

    struct plan9_chain_desc descs[] = {
        {
            .addr = vv_virt_to_phys(request),
            .len = 5,
            .flags = 0,
        },
        {
            .addr = vv_virt_to_phys((uint8_t *)request + 5),
            .len = sizeof(*request) - 5,
            .flags = 0,
        },
        {
            .addr = vv_virt_to_phys(response),
            .len = PLAN9_RESPONSE_SIZE,
            .flags = VRING_DESC_F_WRITE,
        },
    };
    uint32_t used_len = 0;
    test_result_t result =
        plan9_submit_chain(&ctx, descs, 3, PLAN9_RESPONSE_SIZE, &used_len);
    if (result != TEST_PASS)
        return result;

    return plan9_validate_version_response(response, used_len);
}

REGISTER_TEST(PLAN90011, VIRTIO_PCI_DEVICE_9P, test,
              "Accept Tversion split across request descriptors",
              VIRTIO_SPEC_V1_2, "-");
