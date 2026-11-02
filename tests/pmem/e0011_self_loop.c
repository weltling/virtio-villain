/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0011: Pmem flush self-loop chain.
 *
 * Request descriptor with NEXT pointing back to itself. The
 * device must detect the cycle.
 *
 * Spec 5.19.6.1.
 */
#include "lib/virtio_test.h"

static test_result_t test_pmem_self_loop(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    req->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(E0011, VIRTIO_PCI_DEVICE_PMEM, test_pmem_self_loop,
              "Flush chain self-loop",
              VIRTIO_SPEC_V1_2, "2.7.5");
