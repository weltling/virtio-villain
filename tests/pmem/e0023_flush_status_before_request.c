/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0023: Pmem flush chain with the response descriptor placed
 * before the request descriptor.
 *
 * Spec 5.19.6.1: the chain layout is request (readable), then
 * response (writable). Reorder so the writable response appears
 * first. The device must reject the chain rather than treating
 * the response buffer as a request payload.
 */
#include "lib/virtio_test.h"

static test_result_t test_pmem_status_before_request(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    uint8_t *resp = vv_alloc_pages(1);

    req->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    memset(resp, 0xFF, 4);

    /* Wrong order: writable response first, then readable request. */
    vv_desc(vr, 0, resp, 4, VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, req, sizeof(*req), 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(E0023, VIRTIO_PCI_DEVICE_PMEM,
              test_pmem_status_before_request,
              "Pmem flush chain with response descriptor first",
              VIRTIO_SPEC_V1_2, "5.19.6.1");
