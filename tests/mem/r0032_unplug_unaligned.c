/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0032: virtio-mem UNPLUG with addr unaligned to block_size.
 *
 * Spec 5.15.6.2: addr must be a multiple of block_size for both
 * PLUG and UNPLUG. Submit an UNPLUG with addr offset by one
 * byte. The device must reject the request rather than
 * truncating addr to a valid alignment and unplugging a
 * different physical range.
 */
#include "lib/virtio_test.h"

static test_result_t test_mem_unplug_unaligned(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_mem_req  *req  = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->type      = VIRTIO_MEM_REQ_UNPLUG;
    req->addr      = *(volatile uint64_t *)((uint8_t *)dev->device_cfg + 16) + 1;
    req->nb_blocks = 1;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(R0032, VIRTIO_PCI_DEVICE_MEM, test_mem_unplug_unaligned,
              "UNPLUG with addr unaligned to block_size",
              VIRTIO_SPEC_V1_2, "5.15.6.2");
