/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0001: Virtio-mem state query.
 *
 * Submit a VIRTIO_MEM_REQ_STATE request to query the plug state
 * of a memory block.
 *
 * Spec 5.15.6.2: The driver may query the state of blocks.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_mem_state_query(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
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

    uint64_t req_phys = vv_virt_to_phys(req);
    uint64_t resp_phys = vv_virt_to_phys(resp);

    vring_raw_set_desc(vr, 0, req_phys, sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, resp_phys, sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0001, VIRTIO_PCI_DEVICE_MEM, test_mem_state_query,
              "Query memory block state",
              VIRTIO_SPEC_V1_2, "5.15.6.2");
