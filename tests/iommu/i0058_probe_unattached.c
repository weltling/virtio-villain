/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0058: PROBE an endpoint that has never been ATTACHed.
 *
 * Spec 5.13.6.5 says PROBE returns the endpoint's properties
 * regardless of attach state. Submit PROBE on a fresh endpoint
 * that has not been attached and expect either OK or NOENT;
 * the device must not crash.
 *
 * Spec 5.13.6.5.
 */
#include "lib/virtio_test.h"
#include "lib/virtio_iommu.h"

static test_result_t test_iommu_probe_unattached(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_iommu_req_probe *req = vv_alloc_pages(1);

    req->head.type   = VIRTIO_IOMMU_T_PROBE;
    req->endpoint    = 0;
    req->tail.status = 0xFF;

    uint64_t req_phys = vv_virt_to_phys(req);
    size_t   in_len   = (size_t)((uint8_t *)&req->properties - (uint8_t *)req);
    size_t   out_len  = sizeof(req->properties) + sizeof(req->tail);

    vring_raw_set_desc(vr, 0, req_phys, in_len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, req_phys + in_len, out_len,
                       VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(I0058, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_probe_unattached,
              "Probe on never-attached endpoint",
              VIRTIO_SPEC_V1_2, "5.13.6.5");
