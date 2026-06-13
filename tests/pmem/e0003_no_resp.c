/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0003: Pmem flush with truncated request (no response descriptor).
 *
 * Submit a request without a device-writable response descriptor.
 * The device must detect the malformed chain.
 *
 * Spec 5.10.6.1: Robustness test.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pmem_no_resp(struct virtio_dev *dev,
                                       struct vring *vr)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    req->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;

    uint64_t req_phys = vv_virt_to_phys(req);

    /* Only a read-only descriptor, no writable response */
    vring_raw_set_desc(vr, 0, req_phys, sizeof(*req), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(E0003, VIRTIO_PCI_DEVICE_PMEM, test_pmem_no_resp,
              "Flush without response descriptor",
              VIRTIO_SPEC_V1_2, "5.10.6.1");
