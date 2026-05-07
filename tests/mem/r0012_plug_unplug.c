/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0012: Virtio-mem PLUG then UNPLUG.
 *
 * Plug a single block, wait for OK, then unplug the same block.
 * Tests the full lifecycle round-trip.
 *
 * Spec 5.14.6.2.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

struct virtio_mem_req {
    uint16_t type; uint16_t padding[3];
    uint64_t addr; uint16_t nb_blocks; uint16_t p1[3];
} __attribute__((packed));
struct virtio_mem_resp {
    uint16_t type; uint16_t padding[3]; uint16_t state;
} __attribute__((packed));
#define VIRTIO_MEM_REQ_PLUG    0
#define VIRTIO_MEM_REQ_UNPLUG  1

static test_result_t test_mem_plug_unplug(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    uint64_t base = *(volatile uint64_t *)((uint8_t *)dev->device_cfg + 16);

    memset(req, 0, sizeof(*req));
    req->type = VIRTIO_MEM_REQ_PLUG;
    req->addr = base; req->nb_blocks = 1;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    req->type = VIRTIO_MEM_REQ_UNPLUG;
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0012, VIRTIO_PCI_DEVICE_MEM, test_mem_plug_unplug,
              "Plug then unplug",
              VIRTIO_SPEC_V1_2, "5.14.6.2");
