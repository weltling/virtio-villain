/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0025: virtio_mem PLUG with zero nb_blocks.
 *
 * Spec 5.15.6: Submit a PLUG request with nb_blocks=0. The device
 * must reject the degenerate request or handle it gracefully
 * without plugging anything.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_mem_plug_zero_blocks(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    memset(resp, 0xFF, sizeof(*resp));

    volatile uint8_t *cfg = (volatile uint8_t *)dev->device_cfg;
    uint64_t region_addr = *(volatile uint64_t *)(cfg + 16);

    req->type = VIRTIO_MEM_REQ_PLUG;
    req->addr = region_addr;
    req->nb_blocks = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0025, VIRTIO_PCI_DEVICE_MEM, test_mem_plug_zero_blocks,
              "PLUG request with nb_blocks zero",
              VIRTIO_SPEC_V1_2, "5.15.6");
