/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0025: virtio_mem PLUG with zero nb_blocks.
 *
 * Spec 5.15.6: Submit a PLUG request with nb_blocks=0. The device
 * must reject the degenerate request or handle it gracefully
 * without plugging anything.
 */
#include "lib/virtio_test.h"

static test_result_t test_mem_plug_zero_blocks(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(resp, 0xFF, sizeof(*resp));

    volatile uint8_t *cfg = (volatile uint8_t *)dev->device_cfg;
    uint64_t region_addr = *(volatile uint64_t *)(cfg + 16);

    req->type = VIRTIO_MEM_REQ_PLUG;
    req->addr = region_addr;
    req->nb_blocks = 0;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(R0025, VIRTIO_PCI_DEVICE_MEM, test_mem_plug_zero_blocks,
              "PLUG request with nb_blocks zero",
              VIRTIO_SPEC_V1_2, "5.15.6");
