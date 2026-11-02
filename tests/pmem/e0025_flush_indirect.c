/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0025: Pmem flush submitted through an INDIRECT descriptor.
 *
 * Spec 2.7.5.3, 5.19.6.1: A driver may submit a request through
 * a single avail entry that points to an indirect descriptor
 * table holding the readable request and writable response.
 * Build a two entry indirect table for a FLUSH and dispatch
 * from one outer descriptor with VRING_DESC_F_INDIRECT set.
 * The device must accept the indirect form and complete the
 * flush normally.
 */
#include "lib/virtio_test.h"

static test_result_t test_pmem_flush_indirect(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_pmem_req  *req  = vv_alloc_pages(1);
    struct virtio_pmem_resp *resp = vv_alloc_pages(1);
    struct ind_desc *itab = vv_alloc_pages(1);

    req->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    resp->ret = 0xFFFFFFFFU;
    memset(itab, 0, sizeof(*itab) * 2);

    itab[0].addr  = vv_virt_to_phys(req);
    itab[0].len   = sizeof(*req);
    itab[0].flags = VRING_DESC_F_NEXT;
    itab[0].next  = 1;
    itab[1].addr  = vv_virt_to_phys(resp);
    itab[1].len   = sizeof(*resp);
    itab[1].flags = VRING_DESC_F_WRITE;
    itab[1].next  = 0;

    vv_desc(vr, 0, itab, (uint32_t)(sizeof(*itab) * 2), VRING_DESC_F_INDIRECT,
            0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(E0025, VIRTIO_PCI_DEVICE_PMEM, test_pmem_flush_indirect,
              "Pmem flush submitted via indirect descriptor",
              VIRTIO_SPEC_V1_2, "5.19.6.1");
