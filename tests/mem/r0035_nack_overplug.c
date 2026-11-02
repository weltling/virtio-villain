/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0035: PLUG larger than requested_size must be NACKed.
 *
 * v1.4 5.15.6.2: The driver MUST NOT request more memory than
 * config->requested_size. Submit a PLUG whose addr lies inside
 * the usable region but whose nb_blocks pushes plugged_size
 * above requested_size. The device must reject with NACK or
 * ERROR rather than silently grow plugged_size.
 */
#include "lib/virtio_test.h"

static test_result_t test_mem_nack_overplug(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_mem_req  *req  = vv_alloc_pages(1);
    struct virtio_mem_resp_short *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->type      = VIRTIO_MEM_REQ_PLUG;
    req->addr      = 0;
    req->nb_blocks = 0xFFFF;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(R0035, VIRTIO_PCI_DEVICE_MEM, test_mem_nack_overplug,
              "PLUG exceeding requested_size must be NACKed",
              VIRTIO_SPEC_V1_4, "5.15.6.2");
