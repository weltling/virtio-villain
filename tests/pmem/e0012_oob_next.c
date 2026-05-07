/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0012: Pmem flush OOB next index.
 *
 * Request descriptor NEXT points to slot 0xFFFE. Device must
 * reject the chain rather than dereference.
 *
 * Spec 2.7.5.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

struct virtio_pmem_req { uint32_t type; } __attribute__((packed));

static test_result_t test_pmem_oob_next(struct virtio_dev *dev,
                                        struct vring *vr)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    req->type = 0;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 0xFFFE);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(E0012, VIRTIO_PCI_DEVICE_PMEM, test_pmem_oob_next,
              "Flush OOB next index",
              VIRTIO_SPEC_V1_2, "2.7.5");
