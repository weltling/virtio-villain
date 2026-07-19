/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0024: Pmem flush chain with two request descriptors before
 * the writable response.
 *
 * Spec 5.19.6.1: A flush chain is exactly one readable request
 * followed by one writable response. Submit a chain with two
 * readable request descriptors followed by a single writable
 * response. The device must reject the malformed chain rather
 * than picking one of the two requests or treating the second
 * descriptor as response storage.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pmem_two_requests_one_response(struct virtio_dev *dev,
                                                         struct vring *vr)
{
    struct virtio_pmem_req *r1 = vv_alloc_pages(1);
    struct virtio_pmem_req *r2 = vv_alloc_pages(1);
    uint8_t *resp = vv_alloc_pages(1);

    r1->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    r2->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    memset(resp, 0xFF, 4);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(r1), sizeof(*r1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(r2), sizeof(*r2),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(resp), 4,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(E0024, VIRTIO_PCI_DEVICE_PMEM,
              test_pmem_two_requests_one_response,
              "Flush chain with two requests then one response",
              VIRTIO_SPEC_V1_2, "5.19.6.1");
