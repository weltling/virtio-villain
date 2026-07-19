/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0004: Virtio-mem unplug_all request.
 *
 * Submit a VIRTIO_MEM_REQ_UNPLUG_ALL request to unplug all blocks.
 *
 * Spec 5.14.6.2: The device unplugs all currently plugged blocks.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_mem_unplug_all(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    req->type = VIRTIO_MEM_REQ_UNPLUG_ALL;
    req->addr = 0;
    req->nb_blocks = 0;

    memset(resp, 0xFF, sizeof(*resp));

    uint64_t req_phys = vv_virt_to_phys(req);
    uint64_t resp_phys = vv_virt_to_phys(resp);

    vring_raw_set_desc(vr, 0, req_phys, sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, resp_phys, sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0004, VIRTIO_PCI_DEVICE_MEM, test_mem_unplug_all,
              "Unplug all memory blocks",
              VIRTIO_SPEC_V1_2, "5.15.6.2");
