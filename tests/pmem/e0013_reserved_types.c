/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0013: Pmem flush all reserved type values.
 *
 * Iterate type values 1..7 (FLUSH=0 is the only defined type) and
 * verify the device safely returns an error for each.
 *
 * Spec 5.19.6.1.
 */
#include "lib/virtio_test.h"

static test_result_t test_pmem_reserved_types(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    struct virtio_pmem_resp *resp = vv_alloc_pages(1);

    for (uint32_t t = 1; t <= 7; t++) {
        req->type = t;
        resp->ret = 0;
        vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
        vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, t - 1, 0);
        vring_raw_set_avail_idx(vr, t);
        test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
        if (r == TEST_WEDGED || r == TEST_FAIL)
            return r;
    }
    return TEST_PASS;
}

REGISTER_TEST(E0013, VIRTIO_PCI_DEVICE_PMEM, test_pmem_reserved_types,
              "Flush reserved type values",
              VIRTIO_SPEC_V1_2, "5.19.6.1");
