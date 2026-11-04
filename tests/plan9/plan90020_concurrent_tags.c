/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PLAN90020: submit two valid requests with distinct tags together.
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

    uint16_t tag_a = 0;
    uint16_t tag_b = 0;
    struct p9_getattr *request_a =
        plan9_build_getattr(&ctx, 1, &tag_a);
    struct p9_getattr *request_b =
        plan9_build_getattr(&ctx, 1, &tag_b);
    uint8_t *response_a = plan9_alloc(&ctx, PLAN9_RESPONSE_SIZE);
    uint8_t *response_b = plan9_alloc(&ctx, PLAN9_RESPONSE_SIZE);
    if (!request_a || !request_b || !response_a || !response_b)
        TFAIL("test buffer exhausted");

    uint16_t head_a = ctx.next_desc;
    uint16_t head_b = head_a + 2;
    if (head_b + 1 >= vr->size)
        TFAIL("test exhausted descriptor table");

    vv_desc(vr, head_a, request_a, sizeof(*request_a), VRING_DESC_F_NEXT,
            head_a + 1);
    vv_desc(vr, head_a + 1, response_a, PLAN9_RESPONSE_SIZE, VRING_DESC_F_WRITE,
            0);
    vv_desc(vr, head_b, request_b, sizeof(*request_b), VRING_DESC_F_NEXT,
            head_b + 1);
    vv_desc(vr, head_b + 1, response_b, PLAN9_RESPONSE_SIZE, VRING_DESC_F_WRITE,
            0);

    uint16_t used_before = vr->used->idx;
    vring_raw_set_avail(vr, ctx.next_avail % vr->size, head_a);
    vring_raw_set_avail(vr, (ctx.next_avail + 1) % vr->size, head_b);
    ctx.next_avail += 2;
    ctx.next_desc += 4;
    vring_raw_set_avail_idx(vr, ctx.next_avail);

    result = vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
    if (result != TEST_PASS)
        return result;

    uint32_t len_a = 0;
    uint32_t len_b = 0;
    for (uint16_t i = 0; i < 2; i++) {
        struct vring_used_elem *used =
            &vr->used->ring[(used_before + i) % vr->size];
        if (used->len > PLAN9_RESPONSE_SIZE)
            TFAIL("used len=%u exceeds response capacity", used->len);
        if (used->id == head_a) {
            if (len_a != 0)
                TFAIL("request A completed twice");
            len_a = used->len;
        } else if (used->id == head_b) {
            if (len_b != 0)
                TFAIL("request B completed twice");
            len_b = used->len;
        } else {
            TFAIL("unexpected used id=%u", used->id);
        }
    }

    if (!plan9_response_matches(response_a, len_a, P9_RGETATTR, tag_a))
        TFAIL("request A response tag or type is invalid");
    if (!plan9_response_matches(response_b, len_b, P9_RGETATTR, tag_b))
        TFAIL("request B response tag or type is invalid");

    return TEST_PASS;
}

REGISTER_TEST(PLAN90020, VIRTIO_PCI_DEVICE_9P, test,
              "Preserve tags across concurrent requests",
              VIRTIO_SPEC_V1_2, "-");
