/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0014: Virtio-mem STATE_QUERY out-of-region.
 *
 * Query state for an address well beyond the configured region.
 * Device must respond with INVALID_REQUEST.
 *
 * Spec 5.15.6.2.
 */
#include "lib/virtio_test.h"

static test_result_t test_mem_state_oob(struct virtio_dev *dev,
                                        struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    req->type = VIRTIO_MEM_REQ_STATE;
    req->addr = 0xFFFFFFFF00000000ULL;
    req->nb_blocks = 1;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(R0014, VIRTIO_PCI_DEVICE_MEM, test_mem_state_oob,
              "State query out-of-region",
              VIRTIO_SPEC_V1_2, "5.15.6.2");
