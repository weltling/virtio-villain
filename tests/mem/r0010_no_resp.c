/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0010: Virtio-mem missing response descriptor.
 *
 * Submit a plug request without a writable response chain. The
 * device cannot return its response and must reject the chain
 * rather than crash.
 *
 * Spec 5.15.6.2.
 */
#include "lib/virtio_test.h"

static test_result_t test_mem_no_resp(struct virtio_dev *dev,
                                      struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    req->type = VIRTIO_MEM_REQ_PLUG;
    req->addr = *(volatile uint64_t *)((uint8_t *)dev->device_cfg + 16);
    req->nb_blocks = 1;

    vv_desc(vr, 0, req, sizeof(*req), 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(R0010, VIRTIO_PCI_DEVICE_MEM, test_mem_no_resp,
              "Plug without response descriptor",
              VIRTIO_SPEC_V1_2, "5.15.6.2");
