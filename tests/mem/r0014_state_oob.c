/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0014: Virtio-mem STATE_QUERY out-of-region.
 *
 * Query state for an address well beyond the configured region.
 * Device must respond with INVALID_REQUEST.
 *
 * Spec 5.14.6.2.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_mem_state_oob(struct virtio_dev *dev,
                                        struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    req->type = VIRTIO_MEM_REQ_STATE;
    req->addr = 0xFFFFFFFF00000000ULL;
    req->nb_blocks = 1;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0014, VIRTIO_PCI_DEVICE_MEM, test_mem_state_oob,
              "State query out-of-region",
              VIRTIO_SPEC_V1_2, "5.15.6.2");
