/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0006: Virtio-mem PLUG with addr=0.
 *
 * Plug request whose addr is 0, almost certainly outside the
 * advertised mem region. Per spec 5.15.6.2 the device must
 * respond with VIRTIO_MEM_RESP_INVALID_REQUEST or _ERROR.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_mem_plug_zero(struct virtio_dev *dev,
                                        struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    req->type = VIRTIO_MEM_REQ_PLUG;
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

REGISTER_TEST(R0006, VIRTIO_PCI_DEVICE_MEM, test_mem_plug_zero,
              "Plug at address 0",
              VIRTIO_SPEC_V1_2, "5.15.6.2");
