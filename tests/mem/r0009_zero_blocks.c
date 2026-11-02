/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0009: Virtio-mem PLUG nb_blocks=0.
 *
 * Plug request with nb_blocks=0 is meaningless. Device must
 * either reject or treat as no-op without corrupting state.
 *
 * Spec 5.15.6.2.
 */
#include "lib/virtio_test.h"

static test_result_t test_mem_zero_blocks(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    req->type = VIRTIO_MEM_REQ_PLUG;
    req->addr = *(volatile uint64_t *)((uint8_t *)dev->device_cfg + 16);
    req->nb_blocks = 0;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(R0009, VIRTIO_PCI_DEVICE_MEM, test_mem_zero_blocks,
              "Plug with nb_blocks=0",
              VIRTIO_SPEC_V1_2, "5.15.6.2");
