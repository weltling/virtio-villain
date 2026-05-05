/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0055: MAP with MMIO flag without F_MMIO negotiated.
 *
 * The MMIO map flag is gated on the F_MMIO feature bit. Submit
 * a MAP with MAP_F_MMIO set when the driver did not negotiate
 * F_MMIO. The device must reject with UNSUPP or RANGE per spec
 * 5.13.6.3 and 5.13.3.
 *
 * Spec 5.13.6.3, 5.13.3.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_map_mmio_no_feature(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_iommu_req_map *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->head.type   = VIRTIO_IOMMU_T_MAP;
    req->domain      = 0;
    req->virt_start  = 0xC0000;
    req->virt_end    = 0xC0FFF;
    req->phys_start  = 0x90000;
    req->flags       = VIRTIO_IOMMU_MAP_F_READ |
                       VIRTIO_IOMMU_MAP_F_MMIO;
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

REGISTER_TEST(I0055, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_map_mmio_no_feature,
              "Map with MMIO flag without F_MMIO negotiated",
              VIRTIO_SPEC_V1_2, "5.13.6.3");
