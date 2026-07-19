/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0029: virtio_mem PLUG and UNPLUG same block in one batch.
 *
 * Spec 5.14.6: Submit a PLUG and then an UNPLUG for the same
 * block address in the same avail ring batch. The device must
 * handle concurrent conflicting operations gracefully.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_mem_plug_unplug_batch(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_mem_req *req1 = vv_alloc_pages(1);
    struct virtio_mem_resp *resp1 = vv_alloc_pages(1);
    struct virtio_mem_req *req2 = vv_alloc_pages(1);
    struct virtio_mem_resp *resp2 = vv_alloc_pages(1);
    memset(req1, 0, sizeof(*req1));
    memset(resp1, 0xFF, sizeof(*resp1));
    memset(req2, 0, sizeof(*req2));
    memset(resp2, 0xFF, sizeof(*resp2));

    volatile uint8_t *cfg = (volatile uint8_t *)dev->device_cfg;
    uint64_t region_addr = *(volatile uint64_t *)(cfg + 16);

    /* Request 1: PLUG */
    req1->type = VIRTIO_MEM_REQ_PLUG;
    req1->addr = region_addr;
    req1->nb_blocks = 1;

    /* Request 2: UNPLUG same address */
    req2->type = VIRTIO_MEM_REQ_UNPLUG;
    req2->addr = region_addr;
    req2->nb_blocks = 1;

    /* Chain 1: desc 0 -> desc 1 */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req1), sizeof(*req1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp1), sizeof(*resp1),
                       VRING_DESC_F_WRITE, 0);

    /* Chain 2: desc 2 -> desc 3 */
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(req2), sizeof(*req2),
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(resp2), sizeof(*resp2),
                       VRING_DESC_F_WRITE, 0);

    /* Publish both at once */
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0029, VIRTIO_PCI_DEVICE_MEM, test_mem_plug_unplug_batch,
              "PLUG and UNPLUG same block in one batch",
              VIRTIO_SPEC_V1_2, "5.15.6");
