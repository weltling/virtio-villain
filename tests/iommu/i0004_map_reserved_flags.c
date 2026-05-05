/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0004: IOMMU MAP with undefined flag bits set.
 *
 * Set every reserved bit in the flags field. The device must
 * either ignore them silently or reject the request with UNSUPP,
 * but never crash.
 *
 * Spec 5.13.6.3.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_map_reserved_flags(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_iommu_req_map *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->head.type   = VIRTIO_IOMMU_T_MAP;
    req->domain      = 0;
    req->virt_start  = 0x000000;
    req->virt_end    = 0x000FFF;
    req->phys_start  = 0x200000;
    req->flags       = 0xFFFFFFF8;
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

REGISTER_TEST(I0004, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_map_reserved_flags,
              "Map with undefined flag bits set",
              VIRTIO_SPEC_V1_2, "5.13.6.3");
