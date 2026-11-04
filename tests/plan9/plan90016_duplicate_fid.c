/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PLAN90016: attach the same fid twice.
 */
#include "lib/plan9_util.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    struct plan9_test_ctx ctx;
    struct p9_qid qid;
    plan9_test_init(&ctx, dev, vr);

    test_result_t result = plan9_version(&ctx);
    if (result != TEST_PASS)
        return result;
    result = plan9_attach_root(&ctx, 1, &qid);
    if (result != TEST_PASS)
        return result;

    uint16_t tag;
    uint32_t request_len;
    uint8_t *request =
        plan9_build_attach(&ctx, 1, &tag, &request_len);
    uint8_t *response = plan9_alloc(&ctx, PLAN9_RESPONSE_SIZE);
    if (!request || !response)
        TFAIL("test buffer exhausted");

    uint32_t used_len = 0;
    result = plan9_transact(&ctx, request, request_len, 0,
                            response, PLAN9_RESPONSE_SIZE,
                            VRING_DESC_F_WRITE, &used_len);
    if (result != TEST_PASS)
        return result;
    if (!plan9_response_matches(response, used_len, P9_RLERROR, tag))
        TFAIL("duplicate fid did not return Rlerror");

    return plan9_version(&ctx);
}

REGISTER_TEST(PLAN90016, VIRTIO_PCI_DEVICE_9P, test,
              "Reject a duplicate fid attachment",
              VIRTIO_SPEC_V1_2, "-");
