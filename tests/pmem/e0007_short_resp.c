/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0007: Pmem flush short response.
 *
 * Writable response of only 1 byte, less than the 4-byte
 * virtio_pmem_resp.ret. Device must reject.
 *
 * Spec 5.19.6.1.
 */
#include "lib/virtio_test.h"

static test_result_t test_pmem_short_resp(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    req->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    void *r = vv_alloc_pages(1);

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, r, 1, VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(E0007, VIRTIO_PCI_DEVICE_PMEM, test_pmem_short_resp,
              "Flush short response",
              VIRTIO_SPEC_V1_2, "5.19.6.1");
