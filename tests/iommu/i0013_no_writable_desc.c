/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0013: IOMMU request without any writable descriptor.
 *
 * Submit an ATTACH where the entire chain is read-only: no place
 * for the device to write the status byte. The device must
 * detect this and either reject the request silently or report
 * an error, but must not crash.
 *
 * Spec 5.13.6, 2.7.5.3.1.
 */
#include "lib/virtio_test.h"
#include "lib/virtio_iommu.h"

static test_result_t test_iommu_no_writable_desc(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_iommu_req_attach *req = vv_alloc_pages(1);

    req->head.type = VIRTIO_IOMMU_T_ATTACH;

    /* Single read-only descriptor covering the entire request body. */
    vv_desc(vr, 0, req, sizeof(*req), 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(I0013, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_no_writable_desc,
              "Request chain with no writable descriptor",
              VIRTIO_SPEC_V1_2, "5.13.6");
