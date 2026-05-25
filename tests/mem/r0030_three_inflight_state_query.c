/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0030: Three concurrent virtio_mem STATE queries.
 *
 * Spec 5.14.6.2: Push three STATE requests in one avail batch.
 * The device must produce three independent responses without
 * mixing buffers or duplicating completions.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

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

static test_result_t test_mem_three_state(struct virtio_dev *dev,
                                          struct vring *vr)
{
    uint64_t region_addr =
        *(volatile uint64_t *)((uint8_t *)dev->device_cfg + 16);

    for (int i = 0; i < 3; i++) {
        struct virtio_mem_req *req = vv_alloc_pages(1);
        struct virtio_mem_resp *resp = vv_alloc_pages(1);
        memset(req, 0, sizeof(*req));
        memset(resp, 0xFF, sizeof(*resp));

        req->type      = VIRTIO_MEM_REQ_STATE;
        req->addr      = region_addr;
        req->nb_blocks = 1;

        uint16_t d0 = (uint16_t)(i * 2);
        vring_raw_set_desc(vr, d0, vv_virt_to_phys(req), sizeof(*req),
                           VRING_DESC_F_NEXT, d0 + 1);
        vring_raw_set_desc(vr, d0 + 1, vv_virt_to_phys(resp), sizeof(*resp),
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, (uint16_t)i, d0);
    }
    vring_raw_set_avail_idx(vr, 3);

    return vv_kick_and_wait_n(dev, vr, 0, 3, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0030, VIRTIO_PCI_DEVICE_MEM, test_mem_three_state,
              "Three concurrent STATE queries in one batch",
              VIRTIO_SPEC_V1_2, "5.14.6.2");
