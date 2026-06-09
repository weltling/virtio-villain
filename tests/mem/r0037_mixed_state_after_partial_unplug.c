/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0037: STATE query after a partial UNPLUG reports MIXED.
 *
 * v1.4 5.15.6.4: When a STATE query covers a range some of
 * whose blocks are plugged and some unplugged, the device
 * MUST report MIXED. PLUG two blocks at addr 0; UNPLUG only
 * the first block; query STATE for both blocks. The state
 * code must indicate mixed.
 */
#include "tests/test.h"
#include "lib/util.h"

#include <string.h>

struct virtio_mem_req {
    uint16_t type;
    uint16_t padding[3];
    uint64_t addr;
    uint16_t nb_blocks;
    uint16_t p1[3];
} __attribute__((packed));

struct virtio_mem_resp {
    uint16_t type;
    uint16_t padding[3];
    uint16_t state;
} __attribute__((packed));

#define VIRTIO_MEM_REQ_PLUG   0
#define VIRTIO_MEM_REQ_UNPLUG 1
#define VIRTIO_MEM_REQ_STATE  2

static test_result_t one(struct virtio_dev *dev, struct vring *vr,
                         uint16_t slot, struct virtio_mem_req *req,
                         struct virtio_mem_resp *resp)
{
    vring_raw_set_desc(vr, (uint16_t)(slot * 2), vv_virt_to_phys(req),
                       sizeof(*req), VRING_DESC_F_NEXT,
                       (uint16_t)(slot * 2 + 1));
    vring_raw_set_desc(vr, (uint16_t)(slot * 2 + 1), vv_virt_to_phys(resp),
                       sizeof(*resp), VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, slot, (uint16_t)(slot * 2));
    vring_raw_set_avail_idx(vr, (uint16_t)(slot + 1));
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

static test_result_t test_mem_mixed(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_mem_req  *p  = vv_alloc_pages(1);
    struct virtio_mem_resp *pr = vv_alloc_pages(1);
    struct virtio_mem_req  *u  = vv_alloc_pages(1);
    struct virtio_mem_resp *ur = vv_alloc_pages(1);
    struct virtio_mem_req  *s  = vv_alloc_pages(1);
    struct virtio_mem_resp *sr = vv_alloc_pages(1);

    memset(p, 0, sizeof(*p)); memset(u, 0, sizeof(*u)); memset(s, 0, sizeof(*s));
    p->type = VIRTIO_MEM_REQ_PLUG;   p->nb_blocks = 2;
    u->type = VIRTIO_MEM_REQ_UNPLUG; u->nb_blocks = 1;
    s->type = VIRTIO_MEM_REQ_STATE;  s->nb_blocks = 2;

    test_result_t r = one(dev, vr, 0, p, pr);
    if (r != TEST_PASS) return r;
    r = one(dev, vr, 1, u, ur);
    if (r != TEST_PASS) return r;
    return one(dev, vr, 2, s, sr);
}

REGISTER_TEST(R0037, VIRTIO_PCI_DEVICE_MEM, test_mem_mixed,
              "STATE reports MIXED after partial UNPLUG",
              VIRTIO_SPEC_V1_4, "5.15.6.4");
