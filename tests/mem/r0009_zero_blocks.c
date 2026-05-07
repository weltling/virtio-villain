/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0009: Virtio-mem PLUG nb_blocks=0.
 *
 * Plug request with nb_blocks=0 is meaningless. Device must
 * either reject or treat as no-op without corrupting state.
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
#define VIRTIO_MEM_REQ_PLUG 0

static test_result_t test_mem_zero_blocks(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    req->type = VIRTIO_MEM_REQ_PLUG;
    req->addr = *(volatile uint64_t *)((uint8_t *)dev->device_cfg + 16);
    req->nb_blocks = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0009, VIRTIO_PCI_DEVICE_MEM, test_mem_zero_blocks,
              "Plug with nb_blocks=0",
              VIRTIO_SPEC_V1_2, "5.14.6.2");
