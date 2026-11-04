/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PLAN90003: attach to the exported root after version negotiation.
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
    if (!(qid.type & P9_QID_TYPE_DIRECTORY))
        TFAIL("root qid type=0x%02x is not a directory", qid.type);

    return TEST_PASS;
}

REGISTER_TEST(PLAN90003, VIRTIO_PCI_DEVICE_9P, test,
              "Attach to the exported 9P root",
              VIRTIO_SPEC_V1_2, "-");
