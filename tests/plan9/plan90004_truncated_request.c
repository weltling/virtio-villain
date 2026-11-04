/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PLAN90004: submit a request shorter than the seven byte 9P header.
 */
#include "lib/plan9_util.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    struct plan9_test_ctx ctx;
    plan9_test_init(&ctx, dev, vr);

    uint8_t *request = plan9_alloc(&ctx, 4);
    uint8_t *response = plan9_alloc(&ctx, PLAN9_RESPONSE_SIZE);
    memset(request, 0xff, 4);

    test_result_t result =
        plan9_transact(&ctx, request, 4, 0,
                       response, PLAN9_RESPONSE_SIZE, VRING_DESC_F_WRITE,
                       NULL);
    if (result != TEST_PASS)
        return result;

    return plan9_version(&ctx);
}

REGISTER_TEST(PLAN90004, VIRTIO_PCI_DEVICE_9P, test,
              "Handle a request shorter than the 9P header",
              VIRTIO_SPEC_V1_2, "-");
