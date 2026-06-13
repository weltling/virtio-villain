/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0027: pmem DISCARD type when feature is offered.
 *
 * v1.4 5.19.4: VIRTIO_PMEM_F_DISCARD lets the driver request
 * a range be discarded (analogous to blk DISCARD). If the
 * feature is offered, submit a DISCARD request and verify
 * completion.
 */
#include "tests/test.h"
#include "lib/virtio_spec.h"
#include "lib/util.h"

#include <string.h>



static test_result_t test_pmem_discard(struct virtio_dev *dev,
                                       struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_PMEM_F_DISCARD)))
        return TEST_SKIP;

    struct virtio_pmem_req  *req  = vv_alloc_pages(1);
    struct virtio_pmem_resp *resp = vv_alloc_pages(1);
    req->type = VIRTIO_PMEM_REQ_TYPE_DISCARD;
    resp->ret = 0xFFFFFFFFu;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(E0027, VIRTIO_PCI_DEVICE_PMEM, test_pmem_discard,
              "pmem DISCARD when feature is offered",
              VIRTIO_SPEC_V1_4, "5.19.4");
