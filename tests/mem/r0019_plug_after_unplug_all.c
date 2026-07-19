/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0019: Plug immediately after unplug_all.
 *
 * Spec 5.15.6.1: After an UNPLUG_ALL succeeds the entire region
 * is unplugged. A subsequent PLUG of the first block must be
 * accepted. Verify the device handles the transition correctly.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_mem_plug_after_unplug_all(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    volatile uint8_t *cfg = (volatile uint8_t *)dev->device_cfg;
    uint64_t block_size = *(volatile uint64_t *)cfg;
    uint64_t region_addr = *(volatile uint64_t *)(cfg + 16);

    if (block_size == 0)
        return TEST_SKIP;

    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    test_result_t rc;

    /* First: UNPLUG_ALL */
    memset(req, 0, sizeof(*req));
    memset(resp, 0xFF, sizeof(*resp));
    req->type = VIRTIO_MEM_REQ_UNPLUG_ALL;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    rc = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (rc != TEST_PASS)
        return rc;

    /* Second: PLUG the first block */
    memset(req, 0, sizeof(*req));
    memset(resp, 0xFF, sizeof(*resp));
    req->type = VIRTIO_MEM_REQ_PLUG;
    req->addr = region_addr;
    req->nb_blocks = 1;

    vring_raw_set_desc(vr, 2, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait(dev, vr, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0019, VIRTIO_PCI_DEVICE_MEM, test_mem_plug_after_unplug_all,
              "Plug first block after unplug_all",
              VIRTIO_SPEC_V1_2, "5.15.6.1");
