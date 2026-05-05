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
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_no_writable_desc(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_iommu_req_attach *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->head.type = VIRTIO_IOMMU_T_ATTACH;

    uint64_t req_phys = vv_virt_to_phys(req);

    /* Single read-only descriptor covering the entire request body. */
    vring_raw_set_desc(vr, 0, req_phys, sizeof(*req),
                       0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0013, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_no_writable_desc,
              "Request chain with no writable descriptor",
              VIRTIO_SPEC_V1_2, "5.13.6");
