/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0060: ATTACH with BYPASS flag without F_BYPASS_CONFIG.
 *
 * The ATTACH flags include a BYPASS bit gated on the
 * F_BYPASS_CONFIG feature. Without that feature negotiated
 * the device must reject the request with UNSUPP per spec
 * 5.13.5.1.
 *
 * Spec 5.13.5.1.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_attach_bypass_no_feature(struct virtio_dev *dev,
                                                        struct vring *vr)
{
    struct virtio_iommu_req_attach *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->head.type = VIRTIO_IOMMU_T_ATTACH;
    req->domain    = 0;
    req->endpoint  = 0;
    req->flags     = VIRTIO_IOMMU_ATTACH_F_BYPASS;
    req->tail.status = 0xFF;

    uint64_t req_phys = vv_virt_to_phys(req);
    size_t   in_len   = (size_t)((uint8_t *)&req->tail - (uint8_t *)req);

    vring_raw_set_desc(vr, 0, req_phys, in_len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, req_phys + in_len, sizeof(req->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0060, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_attach_bypass_no_feature,
              "Attach with BYPASS flag without F_BYPASS_CONFIG",
              VIRTIO_SPEC_V1_2, "5.13.5.1");
