/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0004: Virtio-mem unplug_all request.
 *
 * Submit a VIRTIO_MEM_REQ_UNPLUG_ALL request to unplug all blocks.
 *
 * Spec 5.15.6.2: The device unplugs all currently plugged blocks.
 */
#include "lib/virtio_test.h"

static test_result_t test_mem_unplug_all(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);

    req->type = VIRTIO_MEM_REQ_UNPLUG_ALL;
    req->addr = 0;
    req->nb_blocks = 0;

    memset(resp, 0xFF, sizeof(*resp));

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(R0004, VIRTIO_PCI_DEVICE_MEM, test_mem_unplug_all,
              "Unplug all memory blocks",
              VIRTIO_SPEC_V1_2, "5.15.6.2");
