/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PLAN90012: split the Rversion buffer across writable descriptors.
 */
#include "lib/plan9_util.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    struct plan9_test_ctx ctx;
    plan9_test_init(&ctx, dev, vr);

    struct p9_version *request = plan9_build_version(&ctx);
    uint8_t *first = plan9_alloc(&ctx, 5);
    uint8_t *second = plan9_alloc(&ctx, PLAN9_RESPONSE_SIZE - 5);
    struct p9_version *response =
        plan9_alloc(&ctx, PLAN9_RESPONSE_SIZE);
    if (!request || !first || !second || !response)
        TFAIL("test buffer exhausted");

    struct plan9_chain_desc descs[] = {
        {
            .addr = vv_virt_to_phys(request),
            .len = sizeof(*request),
            .flags = 0,
        },
        {
            .addr = vv_virt_to_phys(first),
            .len = 5,
            .flags = VRING_DESC_F_WRITE,
        },
        {
            .addr = vv_virt_to_phys(second),
            .len = PLAN9_RESPONSE_SIZE - 5,
            .flags = VRING_DESC_F_WRITE,
        },
    };
    uint32_t used_len = 0;
    test_result_t result =
        plan9_submit_chain(&ctx, descs, 3, PLAN9_RESPONSE_SIZE, &used_len);
    if (result != TEST_PASS)
        return result;

    uint32_t first_len = used_len < 5 ? used_len : 5;
    memcpy(response, first, first_len);
    if (used_len > 5)
        memcpy((uint8_t *)response + 5, second, used_len - 5);

    return plan9_validate_version_response(response, used_len);
}

REGISTER_TEST(PLAN90012, VIRTIO_PCI_DEVICE_9P, test,
              "Accept Rversion split across response descriptors",
              VIRTIO_SPEC_V1_2, "-");
