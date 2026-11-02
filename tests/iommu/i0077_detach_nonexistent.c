/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0077: iommu_detach_nonexistent_domain
 *
 * Submit DETACH for an endpoint not attached to any domain. Spec
 * 5.13.6.3 says the device must handle detach of an unattached
 * endpoint gracefully. It should return an appropriate status
 * without panicking.
 */
#include "lib/virtio_test.h"
#include "lib/virtio_iommu.h"

static test_result_t test_iommu_detach_none(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_iommu_req_detach *req = vv_alloc_pages(1);

    req->head.type = VIRTIO_IOMMU_T_DETACH;
    req->domain = 9999;
    req->endpoint = 0x0008;
    req->tail.status = 0xFF;

    uint64_t base = vv_virt_to_phys(req);
    size_t in_len = (size_t)((uint8_t *)&req->tail - (uint8_t *)req);

    vring_raw_set_desc(vr, 0, base, (uint32_t)in_len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, base + in_len, sizeof(req->tail),
                       VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(I0077, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_detach_none,
              "DETACH endpoint not attached to any domain",
              VIRTIO_SPEC_V1_2, "5.13.6.3");
