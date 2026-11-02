/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0031: pmem flush verify status is zero (success).
 *
 * Spec 5.19.6: the device responds to VIRTIO_PMEM_REQ_TYPE_FLUSH
 * with ret=0 on success. Issue a flush and assert the response
 * field is zero rather than just checking the used ring advanced.
 */
#include "lib/virtio_test.h"

static test_result_t test_pmem_flush_status(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    struct virtio_pmem_resp *resp = vv_alloc_pages(1);

    req->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    resp->ret = 0xDEAD;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);

    VV_TRY(vv_submit_and_wait(dev, vr, 0));

    if (resp->ret != 0)
        TFAIL("flush returned %u, expected 0 (success)", resp->ret);

    return TEST_PASS;
}

REGISTER_TEST(E0031, VIRTIO_PCI_DEVICE_PMEM, test_pmem_flush_status,
              "Flush response status is zero (success)",
              VIRTIO_SPEC_V1_2, "5.19.6");
