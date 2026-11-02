/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0007: Virtio-mem PLUG with huge nb_blocks.
 *
 * Plug request with nb_blocks=0xFFFF, exceeding any plausible
 * region capacity. Device must reject with INVALID_REQUEST or
 * ERROR rather than corrupt internal state.
 */
#include "lib/virtio_test.h"

static test_result_t test_mem_huge_nb(struct virtio_dev *dev,
                                      struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    req->type = VIRTIO_MEM_REQ_PLUG;
    req->addr = *(volatile uint64_t *)((uint8_t *)dev->device_cfg + 16);
    req->nb_blocks = 0xFFFF;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(R0007, VIRTIO_PCI_DEVICE_MEM, test_mem_huge_nb,
              "Plug with huge nb_blocks",
              VIRTIO_SPEC_V1_2, "5.15.6.2");
