/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0027: pmem DISCARD type when feature is offered.
 *
 * v1.4 5.19.4: VIRTIO_PMEM_F_DISCARD lets the driver request
 * a range be discarded (analogous to blk DISCARD). If the
 * feature is offered, submit a DISCARD request and verify
 * completion.
 */
#include "lib/virtio_test.h"

static test_result_t test_pmem_discard(struct virtio_dev *dev,
                                       struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_PMEM_F_DISCARD))
        return TEST_SKIP;

    struct virtio_pmem_req  *req  = vv_alloc_pages(1);
    struct virtio_pmem_resp *resp = vv_alloc_pages(1);
    req->type = VIRTIO_PMEM_REQ_TYPE_DISCARD;
    resp->ret = 0xFFFFFFFFu;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_REQUIRES(E0027, VIRTIO_PCI_DEVICE_PMEM, test_pmem_discard,
              "pmem DISCARD when feature is offered",
              VIRTIO_SPEC_V1_4, "5.19.4",
              (1ULL << VIRTIO_PMEM_F_DISCARD), 0);
