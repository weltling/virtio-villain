/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0016: mem_state_query_last_byte
 *
 * Query state for an address one block below region_size. Spec
 * 5.14.6.2 says the query must succeed for any address inside
 * the configured region. The device must report a real state
 * rather than treating the last block as out of range.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_mem_state_last_block(struct virtio_dev *dev,
                                               struct vring *vr)
{
    volatile uint8_t *cfg = (volatile uint8_t *)dev->device_cfg;
    uint64_t block_size = *(volatile uint64_t *)cfg;
    uint64_t region_addr = *(volatile uint64_t *)(cfg + 16);
    uint64_t region_size = *(volatile uint64_t *)(cfg + 24);

    if (block_size == 0 || region_size < block_size)
        return TEST_SKIP;

    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    memset(resp, 0xFF, sizeof(*resp));

    req->type = VIRTIO_MEM_REQ_STATE;
    req->addr = region_addr + region_size - block_size;
    req->nb_blocks = 1;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0016, VIRTIO_PCI_DEVICE_MEM, test_mem_state_last_block,
              "State query for the last block of the region",
              VIRTIO_SPEC_V1_2, "5.14.6.2");
