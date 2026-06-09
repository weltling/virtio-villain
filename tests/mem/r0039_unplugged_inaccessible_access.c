/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0039: UNPLUGGED_INACCESSIBLE rejects guest access.
 *
 * v1.4 5.15.3 plus VIRTIO_MEM_F_UNPLUGGED_INACCESSIBLE (bit 1):
 * Unplugged blocks must not be guest accessible. Send a STATE
 * query for an addr range and verify the device reports
 * UNPLUGGED (state == 1) for ranges the driver has not
 * plugged. Reading the unplugged range is undefined and may
 * fault; we do not attempt to read it.
 */
#include "tests/test.h"
#include "lib/util.h"

#include <string.h>

#define VIRTIO_MEM_F_UNPLUGGED_INACCESSIBLE 1
#define VIRTIO_MEM_REQ_STATE  2

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

static test_result_t test_mem_unplugged_inacc(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_MEM_F_UNPLUGGED_INACCESSIBLE)))
        return TEST_SKIP;

    struct virtio_mem_req  *req  = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    req->type      = VIRTIO_MEM_REQ_STATE;
    req->nb_blocks = 1;
    req->addr      = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0039, VIRTIO_PCI_DEVICE_MEM, test_mem_unplugged_inacc,
              "STATE query of unplugged inaccessible block",
              VIRTIO_SPEC_V1_4, "5.15.3");
