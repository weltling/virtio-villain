/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0020: Plug request with reserved bits set.
 *
 * Spec 5.15.6: The padding fields in virtio_mem_req must be
 * zero. Set them to nonzero values and verify the device either
 * rejects the request or still completes without crashing.
 */
#include "lib/virtio_test.h"

static test_result_t test_mem_reserved_bits(struct virtio_dev *dev,
                                            struct vring *vr)
{
    volatile uint8_t *cfg = (volatile uint8_t *)dev->device_cfg;
    uint64_t block_size = *(volatile uint64_t *)cfg;
    uint64_t region_addr = *(volatile uint64_t *)(cfg + 16);

    if (block_size == 0)
        return TEST_SKIP;

    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(req, 0xFF, sizeof(*req)); /* Fill all with 0xFF */
    memset(resp, 0xFF, sizeof(*resp));

    req->type = VIRTIO_MEM_REQ_PLUG;
    req->addr = region_addr;
    req->nb_blocks = 1;
    /* padding fields remain 0xFFFF (nonzero) */

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(R0020, VIRTIO_PCI_DEVICE_MEM, test_mem_reserved_bits,
              "Plug request with reserved padding nonzero",
              VIRTIO_SPEC_V1_2, "5.15.6");
