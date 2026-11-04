/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PLAN90021: validate attributes for the exported root.
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

    uint16_t tag = 0;
    struct p9_getattr *request = plan9_build_getattr(&ctx, 1, &tag);
    struct p9_getattr_response *response =
        plan9_alloc(&ctx, PLAN9_RESPONSE_SIZE);
    if (!request || !response)
        TFAIL("test buffer exhausted");

    uint32_t used_len = 0;
    result = plan9_transact(&ctx, request, sizeof(*request), 0,
                            response, PLAN9_RESPONSE_SIZE,
                            VRING_DESC_F_WRITE, &used_len);
    if (result != TEST_PASS)
        return result;
    if (!plan9_response_matches(response, used_len, P9_RGETATTR, tag))
        TFAIL("invalid Rgetattr header (used len=%u)", used_len);
    if (response->header.size != sizeof(*response))
        TFAIL("Rgetattr size=%u, expected %zu",
              response->header.size, sizeof(*response));
    if (!(response->valid & P9_GETATTR_MODE))
        TFAIL("Rgetattr valid=0x%llx mode=0x%x",
              (unsigned long long)response->valid, response->mode);
    if (memcmp(&response->qid, &root_qid, sizeof(root_qid)) != 0)
        TFAIL("Rgetattr qid %02x:%u:%llu differs from root %02x:%u:%llu",
              response->qid.type, response->qid.version,
              (unsigned long long)response->qid.path,
              root_qid.type, root_qid.version,
              (unsigned long long)root_qid.path);
    if ((response->mode & P9_MODE_TYPE_MASK) != P9_MODE_DIRECTORY)
        TFAIL("root mode 0%o is not a directory", response->mode);

    return TEST_PASS;
}

REGISTER_TEST(PLAN90021, VIRTIO_PCI_DEVICE_9P, test,
              "Validate attributes for the exported root",
              VIRTIO_SPEC_V1_2, "-");
