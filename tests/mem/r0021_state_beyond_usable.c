/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0021: STATE query beyond the usable region size.
 *
 * Spec 5.15.6.2: Query state for an address that is within the
 * region but beyond usable_region_size. The device must respond
 * with UNPLUGGED or reject rather than accessing memory beyond
 * the usable portion.
 */
#include "lib/virtio_test.h"

static test_result_t test_mem_state_beyond_usable(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    volatile uint8_t *cfg = (volatile uint8_t *)dev->device_cfg;
    uint64_t block_size = *(volatile uint64_t *)cfg;
    uint64_t region_addr = *(volatile uint64_t *)(cfg + 16);
    uint64_t region_size = *(volatile uint64_t *)(cfg + 24);
    uint64_t usable_size = *(volatile uint64_t *)(cfg + 32);

    if (block_size == 0 || usable_size >= region_size)
        return TEST_SKIP;

    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(resp, 0xFF, sizeof(*resp));

    req->type = VIRTIO_MEM_REQ_STATE;
    /* First block past usable region size */
    req->addr = region_addr + usable_size;
    req->nb_blocks = 1;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(R0021, VIRTIO_PCI_DEVICE_MEM, test_mem_state_beyond_usable,
              "State query beyond usable_region_size",
              VIRTIO_SPEC_V1_2, "5.15.6.2");
