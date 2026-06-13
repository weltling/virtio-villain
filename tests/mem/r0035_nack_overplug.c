/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0035: PLUG larger than requested_size must be NACKed.
 *
 * v1.4 5.15.6.2: The driver MUST NOT request more memory than
 * config->requested_size. Submit a PLUG whose addr lies inside
 * the usable region but whose nb_blocks pushes plugged_size
 * above requested_size. The device must reject with NACK or
 * ERROR rather than silently grow plugged_size.
 */
#include "tests/test.h"
#include "lib/virtio_spec.h"
#include "lib/util.h"

#include <string.h>

static test_result_t test_mem_nack_overplug(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_mem_req  *req  = vv_alloc_pages(1);
    struct virtio_mem_resp_short *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->type      = VIRTIO_MEM_REQ_PLUG;
    req->addr      = 0;
    req->nb_blocks = 0xFFFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0035, VIRTIO_PCI_DEVICE_MEM, test_mem_nack_overplug,
              "PLUG exceeding requested_size must be NACKed",
              VIRTIO_SPEC_V1_4, "5.15.6.2");
