/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0005: Virtio-mem invalid request type.
 *
 * Submit a request with an undefined type. The device must return
 * an error response without crashing.
 *
 * Spec 5.15.6.2: Robustness test.
 */
#include "lib/virtio_test.h"

static test_result_t test_mem_invalid_type(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);

    req->type = 0xFFFF;  /* invalid */
    req->addr = 0;
    req->nb_blocks = 0;

    memset(resp, 0xFF, sizeof(*resp));

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(R0005, VIRTIO_PCI_DEVICE_MEM, test_mem_invalid_type,
              "Request with undefined type value",
              VIRTIO_SPEC_V1_2, "5.15.6.2");
