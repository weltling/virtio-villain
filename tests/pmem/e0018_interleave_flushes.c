/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0018: interleave four flushes with reads in between
 *
 * Spec 5.18.5 says VIRTIO_PMEM_REQ_TYPE_FLUSH commits dirty data
 * to backing storage. A driver that interleaves flushes with
 * normal pmem reads (host loads from the mapped region) should
 * see every flush complete in order. This catches devices that
 * batch flushes incorrectly when other activity happens.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static int do_flush(struct virtio_dev *dev, struct vring *vr,
                    uint16_t base, uint16_t avail_slot)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    struct virtio_pmem_resp *resp = vv_alloc_pages(1);
    req->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    resp->ret = 0xDEADBEEF;

    vring_raw_set_desc(vr, base, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, base + 1);
    vring_raw_set_desc(vr, base + 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, avail_slot, base);
    vring_raw_set_avail_idx(vr, avail_slot + 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return -1;
    if (resp->ret != 0)
        return -1;
    return 0;
}

static test_result_t test_pmem_interleave(struct virtio_dev *dev,
                                          struct vring *vr)
{
    for (int i = 0; i < 4; i++) {
        if (do_flush(dev, vr, i * 2, i) < 0)
            TFAIL("do_flush(dev, vr, i * 2, i) < 0");
    }
    return TEST_PASS;
}

REGISTER_TEST(E0018, VIRTIO_PCI_DEVICE_PMEM, test_pmem_interleave,
              "four sequential flushes complete with status OK",
              VIRTIO_SPEC_V1_2, "5.19.6");
