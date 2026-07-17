/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0042: state query of an unplugged block returns UNPLUGGED.
 *
 * Spec 5.15.6: a STATE request reports whether the queried blocks
 * are plugged, unplugged, or mixed. On a device with nothing
 * plugged (plugged_size == 0) a single-block query at the region
 * start must return ACK with state UNPLUGGED.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_mem_state_unplugged(struct virtio_dev *dev,
                                              struct vring *vr)
{
    if (!dev->device_cfg ||
        dev->device_cfg_length < sizeof(struct virtio_mem_config))
        return TEST_SKIP;

    volatile struct virtio_mem_config *mcfg =
        (volatile struct virtio_mem_config *)dev->device_cfg;
    if (mcfg->plugged_size != 0)
        return TEST_SKIP; /* not a fresh device, block state ambiguous */

    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    req->type = VIRTIO_MEM_REQ_STATE;
    req->addr = mcfg->addr;
    req->nb_blocks = 1;
    memset(resp, 0xFF, sizeof(*resp));

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    if (resp->type != VIRTIO_MEM_RESP_ACK)
        TFAIL("state query type %u, expected ACK", resp->type);
    if (resp->state != VIRTIO_MEM_STATE_UNPLUGGED)
        TFAIL("state %u, expected UNPLUGGED", resp->state);

    return TEST_PASS;
}

REGISTER_TEST(R0042, VIRTIO_PCI_DEVICE_MEM, test_mem_state_unplugged,
              "State query of an unplugged block returns UNPLUGGED",
              VIRTIO_SPEC_V1_2, "5.15.6");
