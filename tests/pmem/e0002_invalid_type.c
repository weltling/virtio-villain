/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0002: Pmem flush with invalid request type.
 *
 * Submit a request with an undefined type value. The device must
 * return an error or handle gracefully without crashing.
 *
 * Spec 5.10.6.1: Robustness test.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_pmem_req {
    uint32_t type;
} __attribute__((packed));

struct virtio_pmem_resp {
    uint32_t ret;
} __attribute__((packed));

static test_result_t test_pmem_invalid_type(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    struct virtio_pmem_resp *resp = vv_alloc_pages(1);

    req->type = 0xDEAD;  /* invalid request type */
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

REGISTER_TEST(E0002, VIRTIO_PCI_DEVICE_PMEM, test_pmem_invalid_type,
              "Flush request with invalid type",
              VIRTIO_SPEC_V1_2, "5.10.6.1");
