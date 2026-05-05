/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0030: IOMMU request via indirect descriptor.
 *
 * Build a 2-descriptor indirect table (request body, status) and
 * submit it via a single descriptor with VRING_DESC_F_INDIRECT.
 * The device must follow the indirection (when
 * VIRTIO_F_INDIRECT_DESC was negotiated) and process the
 * request normally.
 *
 * Spec 2.7.5.3.1, 5.13.6.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_indirect(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_iommu_req_attach *req = vv_alloc_pages(1);
    struct vring_desc *itab = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    memset(itab, 0, 2 * sizeof(*itab));

    req->head.type   = VIRTIO_IOMMU_T_ATTACH;
    req->tail.status = 0xFF;

    uint64_t req_phys = vv_virt_to_phys(req);
    size_t   in_len   = (size_t)((uint8_t *)&req->tail - (uint8_t *)req);
    uint64_t tail_phys = req_phys + in_len;

    itab[0].addr  = req_phys;
    itab[0].len   = in_len;
    itab[0].flags = VRING_DESC_F_NEXT;
    itab[0].next  = 1;
    itab[1].addr  = tail_phys;
    itab[1].len   = sizeof(req->tail);
    itab[1].flags = VRING_DESC_F_WRITE;
    itab[1].next  = 0;

    uint64_t itab_phys = vv_virt_to_phys(itab);
    vring_raw_set_desc(vr, 0, itab_phys, 2 * sizeof(*itab),
                       VRING_DESC_F_INDIRECT, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0030, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_indirect,
              "Submit request through indirect descriptor",
              VIRTIO_SPEC_V1_2, "2.7.5.3.1");
