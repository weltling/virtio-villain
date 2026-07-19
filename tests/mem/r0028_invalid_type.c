/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0028: virtio_mem invalid request type.
 *
 * Spec 5.14.6: Submit a request with type=0xFFFF which is not a
 * defined virtio_mem operation. The device must return an error
 * response without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_mem_invalid_type(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    memset(resp, 0xFF, sizeof(*resp));

    req->type = 0xFFFF;
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

REGISTER_TEST(R0028, VIRTIO_PCI_DEVICE_MEM, test_mem_invalid_type,
              "Request with invalid type 0xFFFF",
              VIRTIO_SPEC_V1_2, "5.15.6");
