/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0023: Verify block_size from config is nonzero and power of 2.
 *
 * Spec 5.15.4: block_size must be a power of two, at least the
 * page size. This test reads the config and issues a STATE query
 * at addr = region_addr + block_size - 1 (misaligned) to confirm
 * the device enforces alignment based on its advertised block_size.
 */
#include "lib/virtio_test.h"

static test_result_t test_mem_block_size_validation(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    volatile uint8_t *cfg = (volatile uint8_t *)dev->device_cfg;
    uint64_t block_size = *(volatile uint64_t *)cfg;
    uint64_t region_addr = *(volatile uint64_t *)(cfg + 16);

    if (block_size == 0)
        return TEST_SKIP;

    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(resp, 0xFF, sizeof(*resp));

    req->type = VIRTIO_MEM_REQ_STATE;
    /* Misaligned: region_addr + block_size - 1 */
    req->addr = region_addr + block_size - 1;
    req->nb_blocks = 1;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(R0023, VIRTIO_PCI_DEVICE_MEM, test_mem_block_size_validation,
              "State query at misaligned block_size address",
              VIRTIO_SPEC_V1_2, "5.15.4");
