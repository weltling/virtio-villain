/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PLAN90018: include a slash in one walk component.
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

    const char *names[] = { "child/name" };
    uint16_t tag;
    uint32_t request_len;
    uint8_t *request =
        plan9_build_walk(&ctx, 1, 2, names, 1, &tag, &request_len);
    uint8_t *response = plan9_alloc(&ctx, PLAN9_RESPONSE_SIZE);
    if (!request || !response)
        TFAIL("test buffer exhausted");

    uint32_t used_len = 0;
    result = plan9_transact(&ctx, request, request_len, 0,
                            response, PLAN9_RESPONSE_SIZE,
                            VRING_DESC_F_WRITE, &used_len);
    if (result != TEST_PASS)
        return result;
    if (plan9_response_matches(response, used_len, P9_RLERROR, tag))
        return TEST_PASS;
    if (!plan9_response_matches(response, used_len, P9_RWALK, tag))
        TFAIL("slash walk returned an invalid response");
    if (used_len < sizeof(struct p9_header) + 2)
        TFAIL("Rwalk is only %u bytes", used_len);

    uint16_t count;
    memcpy(&count, response + sizeof(struct p9_header), sizeof(count));
    if (count != 0)
        TFAIL("slash walk returned %u qids", count);

    return TEST_PASS;
}

REGISTER_TEST(PLAN90018, VIRTIO_PCI_DEVICE_9P, test,
              "Reject a slash inside one walk component",
              VIRTIO_SPEC_V1_2, "-");
