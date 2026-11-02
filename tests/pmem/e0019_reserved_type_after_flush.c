/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0019: pmem_reserved_type_after_flush
 *
 * Submit a normal flush, expect OK, then submit a request whose
 * type is one above the highest defined value. Spec 5.19.6.1
 * defines only TYPE_FLUSH (0); reserved types must yield EIO and
 * the next flush must still complete OK.
 */
#include "lib/virtio_test.h"

static test_result_t test_pmem_reserved_then_flush(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    struct virtio_pmem_resp *resp = vv_alloc_pages(1);

    /* First request: reserved type */
    req->type = 0xCAFE;
    resp->ret = 0xFF;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);

    /* Second request: legitimate flush */
    struct virtio_pmem_req *req2 = vv_alloc_pages(1);
    struct virtio_pmem_resp *resp2 = vv_alloc_pages(1);
    req2->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    resp2->ret = 0xFF;

    vv_desc(vr, 2, req2, sizeof(*req2), VRING_DESC_F_NEXT, 3);
    vv_desc(vr, 3, resp2, sizeof(*resp2), VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(E0019, VIRTIO_PCI_DEVICE_PMEM, test_pmem_reserved_then_flush,
              "Reserved type then a normal flush",
              VIRTIO_SPEC_V1_2, "5.19.6.1");
