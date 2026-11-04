/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PLAN90005: the message header size exceeds the readable descriptor.
 */
#include "lib/plan9_util.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    struct plan9_test_ctx ctx;
    plan9_test_init(&ctx, dev, vr);

    struct p9_header *request = plan9_alloc(&ctx, sizeof(*request));
    uint8_t *response = plan9_alloc(&ctx, PLAN9_RESPONSE_SIZE);
    request->size = 64;
    request->type = P9_TVERSION;
    request->tag = P9_NOTAG;

    test_result_t result =
        plan9_transact(&ctx, request, sizeof(*request), 0,
                       response, PLAN9_RESPONSE_SIZE, VRING_DESC_F_WRITE,
                       NULL);
    if (result != TEST_PASS)
        return result;

    return plan9_version(&ctx);
}

REGISTER_TEST(PLAN90005, VIRTIO_PCI_DEVICE_9P, test,
              "Declared message size exceeds readable descriptor",
              VIRTIO_SPEC_V1_2, "-");
