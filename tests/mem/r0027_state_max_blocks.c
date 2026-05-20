/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0027: virtio_mem STATE query with nb_blocks at maximum.
 *
 * Spec 5.14.6: Submit a STATE request with nb_blocks set to
 * 0xFFFF. The device must clamp or reject the oversized range
 * without integer overflow.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

struct virtio_mem_req {
    uint16_t type;
    uint16_t padding[3];
    uint64_t addr;
    uint16_t nb_blocks;
    uint16_t padding1[3];
} __attribute__((packed));

struct virtio_mem_resp {
    uint16_t type;
    uint16_t padding[3];
    uint16_t state;
} __attribute__((packed));

#define VIRTIO_MEM_REQ_STATE 3

static test_result_t test_mem_state_max_blocks(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    memset(resp, 0xFF, sizeof(*resp));

    volatile uint8_t *cfg = (volatile uint8_t *)dev->device_cfg;
    uint64_t region_addr = *(volatile uint64_t *)(cfg + 16);

    req->type = VIRTIO_MEM_REQ_STATE;
    req->addr = region_addr;
    req->nb_blocks = 0xFFFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0027, VIRTIO_PCI_DEVICE_MEM, test_mem_state_max_blocks,
              "STATE query with maximum nb_blocks value",
              VIRTIO_SPEC_V1_2, "5.14.6");
