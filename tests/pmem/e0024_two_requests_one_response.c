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
#include "lib/virtio_test.h"

static test_result_t test_pmem_two_requests_one_response(struct virtio_dev *dev,
                                                         struct vring *vr)
{
    struct virtio_pmem_req *r1 = vv_alloc_pages(1);
    struct virtio_pmem_req *r2 = vv_alloc_pages(1);
    uint8_t *resp = vv_alloc_pages(1);

    r1->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    r2->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    memset(resp, 0xFF, 4);

    vv_desc(vr, 0, r1, sizeof(*r1), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, r2, sizeof(*r2), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, resp, 4, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(E0024, VIRTIO_PCI_DEVICE_PMEM,
              test_pmem_two_requests_one_response,
              "Flush chain with two requests then one response",
              VIRTIO_SPEC_V1_2, "5.19.6.1");
