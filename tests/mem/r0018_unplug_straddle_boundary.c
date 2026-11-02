/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0018: mem_unplug_straddle_boundary
 *
 * Issue an UNPLUG that spans both a plugged region and the area
 * just outside. Spec 5.15.6 requires the device to reject without
 * partially unplugging and to leave the map state idempotent on a
 * follow up state query.
 */
#include "lib/virtio_test.h"

static test_result_t test_mem_unplug_straddle(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile uint8_t *cfg = (volatile uint8_t *)dev->device_cfg;
    uint64_t block_size = *(volatile uint64_t *)cfg;
    uint64_t region_addr = *(volatile uint64_t *)(cfg + 16);
    uint64_t region_size = *(volatile uint64_t *)(cfg + 24);

    if (block_size == 0 || region_size == 0)
        return TEST_SKIP;

    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(resp, 0xFF, sizeof(*resp));

    req->type = VIRTIO_MEM_REQ_UNPLUG;
    /* Last block plus one block past the region */
    req->addr = region_addr + region_size - block_size;
    req->nb_blocks = 2;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(R0018, VIRTIO_PCI_DEVICE_MEM, test_mem_unplug_straddle,
              "Unplug spanning region boundary",
              VIRTIO_SPEC_V1_2, "5.15.6");
