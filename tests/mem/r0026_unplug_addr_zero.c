/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0026: virtio_mem UNPLUG at address zero.
 *
 * Spec 5.14.6: Submit an UNPLUG request at address 0 which is
 * outside the usable region. The device must reject the invalid
 * address rather than corrupting internal state.
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

#define VIRTIO_MEM_REQ_UNPLUG 2

static test_result_t test_mem_unplug_addr_zero(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    memset(resp, 0xFF, sizeof(*resp));

    req->type = VIRTIO_MEM_REQ_UNPLUG;
    req->addr = 0;
    req->nb_blocks = 1;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0026, VIRTIO_PCI_DEVICE_MEM, test_mem_unplug_addr_zero,
              "UNPLUG at address zero outside usable region",
              VIRTIO_SPEC_V1_2, "5.14.6");
