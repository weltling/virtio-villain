/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0080: IOMMU ATTACH with invalid domain.
 *
 * Spec 5.13.6.3: Submit an ATTACH request with domain_id=0xFFFFFFFF.
 * The device must reject the invalid domain rather than creating an
 * unbounded mapping structure.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_attach_invalid_domain(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);
    memset(page, 0, 4096);

    struct virtio_iommu_req_attach *req = (void *)page;
    req->head.type = VIRTIO_IOMMU_T_ATTACH;
    req->domain    = 0xFFFFFFFFU;
    req->endpoint  = 0;
    req->tail.status = 0xFF;

    uint64_t phys = vv_virt_to_phys(page);
    size_t req_len = sizeof(*req) - sizeof(req->tail);
    size_t tail_off = (size_t)((uint8_t *)&req->tail - (uint8_t *)req);

    vring_raw_set_desc(vr, 0, phys, req_len, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, phys + tail_off, sizeof(req->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0080, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_attach_invalid_domain,
              "IOMMU ATTACH with maximum domain_id",
              VIRTIO_SPEC_V1_2, "5.13.6.3");
