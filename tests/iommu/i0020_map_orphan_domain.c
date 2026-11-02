/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0020: IOMMU MAP to a domain with no attached endpoint.
 *
 * Submit a MAP for a domain id that has never appeared in any
 * ATTACH request. Some implementations create the domain
 * lazily, others reject. Either way the device must not crash.
 *
 * Spec 5.13.5.6 (the spec lets a domain exist before any ATTACH;
 * this stresses the code path).
 */
#include "lib/virtio_test.h"
#include "lib/virtio_iommu.h"

static test_result_t test_iommu_map_orphan_domain(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_iommu_req_map *req = vv_alloc_pages(1);

    req->head.type   = VIRTIO_IOMMU_T_MAP;
    req->domain      = 0xBEEF;
    req->virt_start  = 0;
    req->virt_end    = 0xFFF;
    req->phys_start  = 0x80000;
    req->flags       = VIRTIO_IOMMU_MAP_F_READ;
    req->tail.status = 0xFF;

    uint64_t req_phys = vv_virt_to_phys(req);
    size_t   in_len   = (size_t)((uint8_t *)&req->tail - (uint8_t *)req);
    uint64_t tail_phys = req_phys + in_len;

    vring_raw_set_desc(vr, 0, req_phys, in_len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, tail_phys, sizeof(req->tail),
                       VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(I0020, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_map_orphan_domain,
              "Map to domain with no attached endpoint",
              VIRTIO_SPEC_V1_2, "5.13.5.6");
