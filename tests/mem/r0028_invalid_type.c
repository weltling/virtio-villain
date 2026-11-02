/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0028: virtio_mem invalid request type.
 *
 * Spec 5.15.6: Submit a request with type=0xFFFF which is not a
 * defined virtio_mem operation. The device must return an error
 * response without crashing.
 */
#include "lib/virtio_test.h"

static test_result_t test_mem_invalid_type(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(resp, 0xFF, sizeof(*resp));

    req->type = 0xFFFF;
    req->addr = 0;
    req->nb_blocks = 1;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(R0028, VIRTIO_PCI_DEVICE_MEM, test_mem_invalid_type,
              "Request with invalid type 0xFFFF",
              VIRTIO_SPEC_V1_2, "5.15.6");
