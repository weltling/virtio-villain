/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0026: virtio_mem UNPLUG at address zero.
 *
 * Spec 5.15.6: Submit an UNPLUG request at address 0 which is
 * outside the usable region. The device must reject the invalid
 * address rather than corrupting internal state.
 */
#include "lib/virtio_test.h"

static test_result_t test_mem_unplug_addr_zero(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(resp, 0xFF, sizeof(*resp));

    req->type = VIRTIO_MEM_REQ_UNPLUG;
    req->addr = 0;
    req->nb_blocks = 1;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(R0026, VIRTIO_PCI_DEVICE_MEM, test_mem_unplug_addr_zero,
              "UNPLUG at address zero outside usable region",
              VIRTIO_SPEC_V1_2, "5.15.6");
