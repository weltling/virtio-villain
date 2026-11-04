/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PLAN90019: exceed the maximum walk component count.
 */
#include "lib/plan9_util.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    struct plan9_test_ctx ctx;
    struct p9_qid root_qid;
    plan9_test_init(&ctx, dev, vr);

    test_result_t result = plan9_version(&ctx);
    if (result != TEST_PASS)
        return result;
    result = plan9_attach_root(&ctx, 1, &root_qid);
    if (result != TEST_PASS)
        return result;

    const char *names[P9_MAXWELEM + 1];
    for (uint16_t i = 0; i < P9_MAXWELEM + 1; i++)
        names[i] = "x";

    uint16_t tag = 0;
    uint32_t request_len = 0;
    uint8_t *request =
        plan9_build_walk(&ctx, 1, 2, names, P9_MAXWELEM + 1,
                         &tag, &request_len);
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
        TFAIL("excessive walk count did not return Rlerror");

    return TEST_PASS;
}

REGISTER_TEST(PLAN90019, VIRTIO_PCI_DEVICE_9P, test,
              "Reject more than 16 walk components",
              VIRTIO_SPEC_V1_2, "-");
