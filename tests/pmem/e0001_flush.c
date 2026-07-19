/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0001: Pmem flush request.
 *
 * Submit a VIRTIO_PMEM_REQ_TYPE_FLUSH request and verify the device
 * responds with VIRTIO_PMEM_RESP_TYPE_OK.
 *
 * Spec 5.10.6.1: The driver sends flush requests to persist writes.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pmem_flush(struct virtio_dev *dev,
                                     struct vring *vr)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    struct virtio_pmem_resp *resp = vv_alloc_pages(1);

    req->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    resp->ret = 0xFF;

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

REGISTER_TEST(E0001, VIRTIO_PCI_DEVICE_PMEM, test_pmem_flush,
              "Flush persistent memory",
              VIRTIO_SPEC_V1_2, "5.19.6.1");
