/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0003: Pmem flush with truncated request (no response descriptor).
 *
 * Submit a request without a device-writable response descriptor.
 * The device must detect the malformed chain.
 *
 * Spec 5.19.6.1: Robustness test.
 */
#include "lib/virtio_test.h"

static test_result_t test_pmem_no_resp(struct virtio_dev *dev,
                                       struct vring *vr)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    req->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;

    /* Only a read-only descriptor, no writable response */
    vv_desc(vr, 0, req, sizeof(*req), 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(E0003, VIRTIO_PCI_DEVICE_PMEM, test_pmem_no_resp,
              "Flush without response descriptor",
              VIRTIO_SPEC_V1_2, "5.19.6.1");
