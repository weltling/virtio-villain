/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0005: IOMMU UNMAP of an unmapped range.
 *
 * Request UNMAP on a virt range that was never mapped. The device
 * must respond with NOENT or stay silent, never crash.
 *
 * Spec 5.13.6.4.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_unmap_no_mapping(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_iommu_req_unmap *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->head.type   = VIRTIO_IOMMU_T_UNMAP;
    req->domain      = 0xCAFE;
    req->virt_start  = 0xAA00000;
    req->virt_end    = 0xAA00FFF;
    req->tail.status = 0xFF;

    uint64_t req_phys = vv_virt_to_phys(req);
    size_t   in_len   = (size_t)((uint8_t *)&req->tail - (uint8_t *)req);
    uint64_t tail_phys = req_phys + in_len;

    vring_raw_set_desc(vr, 0, req_phys, in_len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, tail_phys, sizeof(req->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0005, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_unmap_no_mapping,
              "Unmap a range that was never mapped",
              VIRTIO_SPEC_V1_2, "5.13.6.4");
