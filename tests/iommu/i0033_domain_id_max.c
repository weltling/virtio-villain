/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0033: IOMMU ATTACH with domain id beyond device range.
 *
 * If VIRTIO_IOMMU_F_DOMAIN_RANGE was offered, device config
 * exposes domain_range with a maximum domain id. Attach with
 * domain = UINT32_MAX, which is far above any reasonable
 * device-supported maximum. The device must reject with
 * VIRTIO_IOMMU_S_RANGE or INVAL and stay alive.
 *
 * Spec 5.13.5.1, 5.13.4.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_domain_id_max(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_iommu_req_attach *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->head.type = VIRTIO_IOMMU_T_ATTACH;
    req->domain    = 0xFFFFFFFF;
    req->endpoint  = 0;
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

REGISTER_TEST(I0033, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_domain_id_max,
              "Attach with domain id at UINT32_MAX",
              VIRTIO_SPEC_V1_2, "5.13.5.1");
