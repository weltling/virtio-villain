/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0017: mem_plug_overflow
 *
 * Submit a PLUG request whose addr plus nb_blocks times block_size
 * overflows a 64 bit value. Spec 5.15.6 requires bounds checking
 * before the device acts. The device must reject without integer
 * wraparound and stay alive.
 */
#include "lib/virtio_test.h"

static test_result_t test_mem_plug_overflow(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(resp, 0xFF, sizeof(*resp));

    req->type = VIRTIO_MEM_REQ_PLUG;
    req->addr = 0xFFFFFFFFFFFFF000ULL;
    req->nb_blocks = 0xFFFF;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(R0017, VIRTIO_PCI_DEVICE_MEM, test_mem_plug_overflow,
              "Plug with addr plus length overflowing u64",
              VIRTIO_SPEC_V1_2, "5.15.6");
