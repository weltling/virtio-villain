/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0001: Pmem flush request.
 *
 * Submit a VIRTIO_PMEM_REQ_TYPE_FLUSH request and verify the device
 * responds with VIRTIO_PMEM_RESP_TYPE_OK.
 *
 * Spec 5.19.6.1: The driver sends flush requests to persist writes.
 */
#include "lib/virtio_test.h"

static test_result_t test_pmem_flush(struct virtio_dev *dev,
                                     struct vring *vr)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    struct virtio_pmem_resp *resp = vv_alloc_pages(1);

    req->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    resp->ret = 0xFF;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(E0001, VIRTIO_PCI_DEVICE_PMEM, test_pmem_flush,
              "Flush persistent memory",
              VIRTIO_SPEC_V1_2, "5.19.6.1");
