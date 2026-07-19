/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0031: pmem flush verify status is zero (success).
 *
 * Spec 5.16.6: the device responds to VIRTIO_PMEM_REQ_TYPE_FLUSH
 * with ret=0 on success. Issue a flush and assert the response
 * field is zero rather than just checking the used ring advanced.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pmem_flush_status(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    struct virtio_pmem_resp *resp = vv_alloc_pages(1);

    req->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    resp->ret = 0xDEAD;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    if (resp->ret != 0)
        TFAIL("flush returned %u, expected 0 (success)", resp->ret);

    return TEST_PASS;
}

REGISTER_TEST(E0031, VIRTIO_PCI_DEVICE_PMEM, test_pmem_flush_status,
              "Flush response status is zero (success)",
              VIRTIO_SPEC_V1_2, "5.19.6");
