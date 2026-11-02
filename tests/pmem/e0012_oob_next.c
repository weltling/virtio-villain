/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0012: Pmem flush OOB next index.
 *
 * Request descriptor NEXT points to slot 0xFFFE. Device must
 * reject the chain rather than dereference.
 *
 * Spec 2.7.5.
 */
#include "lib/virtio_test.h"

static test_result_t test_pmem_oob_next(struct virtio_dev *dev,
                                        struct vring *vr)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    req->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 0xFFFE);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(E0012, VIRTIO_PCI_DEVICE_PMEM, test_pmem_oob_next,
              "Flush OOB next index",
              VIRTIO_SPEC_V1_2, "2.7.5");
