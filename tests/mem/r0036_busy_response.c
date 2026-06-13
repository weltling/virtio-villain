/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0036: BUSY response is well formed.
 *
 * v1.4 5.15.6: A PLUG submitted while the device is in the
 * middle of another transaction may return BUSY. Submit a
 * PLUG of zero blocks; if BUSY comes back the device must
 * still write a complete response header and not wedge.
 */
#include "tests/test.h"
#include "lib/virtio_spec.h"
#include "lib/util.h"

#include <string.h>

static test_result_t test_mem_busy_response(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_mem_req  *req  = vv_alloc_pages(1);
    struct virtio_mem_resp_short *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    resp->type = 0xFFFF;

    req->type      = VIRTIO_MEM_REQ_PLUG;
    req->addr      = 0;
    req->nb_blocks = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0036, VIRTIO_PCI_DEVICE_MEM, test_mem_busy_response,
              "PLUG with zero blocks; well formed response",
              VIRTIO_SPEC_V1_4, "5.15.6");
