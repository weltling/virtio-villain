/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PLAN90008: submit an unknown 9P2000.L message type.
 */
#include "lib/plan9_util.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    struct plan9_test_ctx ctx;
    plan9_test_init(&ctx, dev, vr);

    test_result_t result = plan9_version(&ctx);
    if (result != TEST_PASS)
        return result;

    struct p9_header *request = plan9_alloc(&ctx, sizeof(*request));
    uint8_t *response = plan9_alloc(&ctx, PLAN9_RESPONSE_SIZE);
    uint16_t tag = ctx.next_tag++;
    request->size = sizeof(*request);
    request->type = 0xff;
    request->tag = tag;

    uint32_t used_len = 0;
    result = plan9_transact(&ctx, request, sizeof(*request), 0,
                            response, PLAN9_RESPONSE_SIZE,
                            VRING_DESC_F_WRITE, &used_len);
    if (result != TEST_PASS)
        return result;
    if (used_len != 0 &&
        !plan9_response_matches(response, used_len, P9_RLERROR, tag))
        TFAIL("unknown message returned invalid error (used len=%u)",
              used_len);

    return plan9_version(&ctx);
}

REGISTER_TEST(PLAN90008, VIRTIO_PCI_DEVICE_9P, test,
              "Unknown message type preserves queue health",
              VIRTIO_SPEC_V1_2, "-");
