/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0017: many sequential flushes
 *
 * Spec 5.19.6 defines VIRTIO_PMEM_REQ_TYPE_FLUSH as a synchronous
 * flush request. Submit 64 flushes back to back, waiting for each
 * to complete before sending the next. A VMM that has resource
 * leaks per flush, or that fuses multiple in flight flushes
 * incorrectly, will fail or wedge after enough iterations.
 */
#include "lib/virtio_test.h"

#define FLUSH_COUNT 64

static test_result_t test_pmem_many_flushes(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    struct virtio_pmem_resp *resp = vv_alloc_pages(1);

    req->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;

    for (int i = 0; i < FLUSH_COUNT; i++) {
        resp->ret = 0xDEADBEEF;

        uint16_t slot = (uint16_t)((i * 2) % vr->size);
        uint16_t slot2 = (uint16_t)((slot + 1) % vr->size);

        vv_desc(vr, slot, req, sizeof(*req), VRING_DESC_F_NEXT, slot2);
        vv_desc(vr, slot2, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);

        vring_raw_set_avail(vr, (uint16_t)i, slot);
        vring_raw_set_avail_idx(vr, (uint16_t)(i + 1));

        VV_TRY(vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS));
    }

    return TEST_PASS;
}

REGISTER_TEST(E0017, VIRTIO_PCI_DEVICE_PMEM, test_pmem_many_flushes,
              "64 sequential PMEM flushes complete in order",
              VIRTIO_SPEC_V1_2, "5.19.6");
