/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0013: Virtio-mem UNPLUG never-plugged block.
 *
 * Send UNPLUG for a block address that was never PLUGged. Device
 * must reject with appropriate error rather than silently drop.
 *
 * Spec 5.14.6.2.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_mem_unplug_unbacked(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    req->type = VIRTIO_MEM_REQ_UNPLUG;
    req->addr = *(volatile uint64_t *)((uint8_t *)dev->device_cfg + 16);
    req->nb_blocks = 1;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0013, VIRTIO_PCI_DEVICE_MEM, test_mem_unplug_unbacked,
              "Unplug never-plugged block",
              VIRTIO_SPEC_V1_2, "5.15.6.2");
