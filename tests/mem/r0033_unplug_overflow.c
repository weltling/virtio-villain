/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0033: virtio-mem UNPLUG with addr + length overflowing u64.
 *
 * Spec 5.15.6.2: A range must lie within the usable region. An
 * UNPLUG whose addr plus nb_blocks*block_size overflows a 64
 * bit value can confuse a device that bounds checks the end
 * address by computing it without overflow detection. Submit
 * UNPLUG with addr near the top of the address space and a
 * large nb_blocks. The device must reject the request rather
 * than walking off the end of its plugged range bitmap.
 */
#include "lib/virtio_test.h"

static test_result_t test_mem_unplug_overflow(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_mem_req  *req  = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->type      = VIRTIO_MEM_REQ_UNPLUG;
    req->addr      = 0xFFFFFFFFFFFFF000ULL;
    req->nb_blocks = 0xFFFF;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(R0033, VIRTIO_PCI_DEVICE_MEM, test_mem_unplug_overflow,
              "UNPLUG with addr plus length overflowing u64",
              VIRTIO_SPEC_V1_2, "5.15.6.2");
