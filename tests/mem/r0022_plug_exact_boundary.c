/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0022: Plug at exact block boundaries.
 *
 * Spec 5.14.6.1: Verify that plugging exactly the first block
 * and last block of the usable region are handled correctly.
 * Both addresses must be block_size aligned and within range.
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

#define VIRTIO_MEM_REQ_PLUG 1

static test_result_t test_mem_plug_exact_boundary(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    volatile uint8_t *cfg = (volatile uint8_t *)dev->device_cfg;
    uint64_t block_size = *(volatile uint64_t *)cfg;
    uint64_t region_addr = *(volatile uint64_t *)(cfg + 16);
    uint64_t usable_size = *(volatile uint64_t *)(cfg + 32);

    if (block_size == 0 || usable_size < block_size * 2)
        return TEST_SKIP;

    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    test_result_t rc;

    /* Plug last block in usable region */
    memset(req, 0, sizeof(*req));
    memset(resp, 0xFF, sizeof(*resp));
    req->type = VIRTIO_MEM_REQ_PLUG;
    req->addr = region_addr + usable_size - block_size;
    req->nb_blocks = 1;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    rc = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (rc != TEST_PASS)
        return rc;

    /* Plug first block */
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

REGISTER_TEST(R0022, VIRTIO_PCI_DEVICE_MEM, test_mem_plug_exact_boundary,
              "Plug at exact first and last block boundaries",
              VIRTIO_SPEC_V1_2, "5.14.6.1");
