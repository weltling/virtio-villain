/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0030: pmem FLUSH response byte variations.
 *
 * v1.4 5.19.6: The response struct's ret field signals success
 * (0) or generic failure (non zero). Submit three FLUSH
 * requests back to back; all responses must come back even
 * when the driver pre fills resp->ret with various sentinels.
 */
#include "tests/test.h"
#include "lib/virtio_spec.h"
#include "lib/util.h"

#include <string.h>


static test_result_t test_pmem_status_variations(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_pmem_req  *req  = vv_alloc_pages(1);
    struct virtio_pmem_resp *resp = vv_alloc_pages(1);

    static const uint32_t sentinels[] = {0, 0xFF, 0xFFFFFFFFu};
    for (unsigned i = 0; i < 3; i++) {
        req->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
        resp->ret = sentinels[i];

        vring_raw_set_desc(vr, (uint16_t)(i * 2),
                           vv_virt_to_phys(req), sizeof(*req),
                           VRING_DESC_F_NEXT, (uint16_t)(i * 2 + 1));
        vring_raw_set_desc(vr, (uint16_t)(i * 2 + 1),
                           vv_virt_to_phys(resp), sizeof(*resp),
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, (uint16_t)i, (uint16_t)(i * 2));
        vring_raw_set_avail_idx(vr, (uint16_t)(i + 1));

        test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
        if (r != TEST_PASS) return r;
    }
    return TEST_PASS;
}

REGISTER_TEST(E0030, VIRTIO_PCI_DEVICE_PMEM, test_pmem_status_variations,
              "pmem FLUSH with pre filled response sentinels",
              VIRTIO_SPEC_V1_4, "5.19.6");
