/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PLAN90015: submit Tattach before Tversion negotiation.
 */
#include "lib/plan9_util.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    struct plan9_test_ctx ctx;
    plan9_test_init(&ctx, dev, vr);

    uint16_t tag;
    uint32_t request_len;
    uint8_t *request =
        plan9_build_attach(&ctx, 1, &tag, &request_len);
    uint8_t *response = plan9_alloc(&ctx, PLAN9_RESPONSE_SIZE);
    if (!request || !response)
        TFAIL("test buffer exhausted");

    uint32_t used_len = 0;
    test_result_t result =
        plan9_transact(&ctx, request, request_len, 0,
                       response, PLAN9_RESPONSE_SIZE, VRING_DESC_F_WRITE,
                       &used_len);
    if (result != TEST_PASS)
        return result;

    if (used_len != 0) {
        if (plan9_response_matches(response, used_len, P9_RATTACH, tag)) {
            if (used_len < sizeof(struct p9_header) +
                           sizeof(struct p9_qid))
                TFAIL("Rattach is only %u bytes", used_len);
            struct p9_qid *qid =
                (struct p9_qid *)(response + sizeof(struct p9_header));
            if (!(qid->type & P9_QID_TYPE_DIRECTORY))
                TFAIL("root qid type=0x%02x is not a directory",
                      qid->type);
        } else if (!plan9_response_matches(response, used_len,
                                           P9_RLERROR, tag)) {
            TFAIL("invalid attach response before Tversion");
        }
    }

    return plan9_version(&ctx);
}

REGISTER_TEST(PLAN90015, VIRTIO_PCI_DEVICE_9P, test,
              "Handle Tattach before version negotiation",
              VIRTIO_SPEC_V1_2, "-");
