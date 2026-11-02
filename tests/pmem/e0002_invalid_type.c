/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0002: Pmem flush with invalid request type.
 *
 * Submit a request with an undefined type value. The device must
 * return an error or handle gracefully without crashing.
 *
 * Spec 5.19.6.1: Robustness test.
 */
#include "lib/virtio_test.h"

static test_result_t test_pmem_invalid_type(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    struct virtio_pmem_resp *resp = vv_alloc_pages(1);

    req->type = 0xDEAD;  /* invalid request type */
    resp->ret = 0xFF;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(E0002, VIRTIO_PCI_DEVICE_PMEM, test_pmem_invalid_type,
              "Flush request with invalid type",
              VIRTIO_SPEC_V1_2, "5.19.6.1");
