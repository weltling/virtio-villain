/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0001: Virtio-mem state query.
 *
 * Submit a VIRTIO_MEM_REQ_STATE request to query the plug state
 * of a memory block.
 *
 * Spec 5.15.6.2: The driver may query the state of blocks.
 */
#include "lib/virtio_test.h"

static test_result_t test_mem_state_query(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);

    req->type = VIRTIO_MEM_REQ_STATE;
    /* Query from device config start address — read from device_cfg */
    volatile uint64_t *cfg = (volatile uint64_t *)dev->device_cfg;
    /* VirtioMemConfig: block_size(8), node_id(2), padding(6), addr(8) */
    uint64_t block_size = cfg[0];
    uint64_t region_addr = *(volatile uint64_t *)((uint8_t *)dev->device_cfg + 16);
    req->addr = region_addr;
    req->nb_blocks = 1;
    (void)block_size;

    memset(resp, 0xFF, sizeof(*resp));

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(R0001, VIRTIO_PCI_DEVICE_MEM, test_mem_state_query,
              "Query memory block state",
              VIRTIO_SPEC_V1_2, "5.15.6.2");
