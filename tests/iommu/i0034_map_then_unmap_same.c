/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0034: IOMMU concurrent MAP and UNMAP of the same range in
 * a single batch.
 *
 * Place a MAP and an UNMAP for the same domain and virtual
 * range back-to-back in the available ring before kicking. The
 * device may execute them in submission order, but it must do
 * so atomically and not crash.
 *
 * Spec 5.13.6.3, 5.13.6.4.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_map_then_unmap_same(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_iommu_req_map   *r1 = vv_alloc_pages(1);
    struct virtio_iommu_req_unmap *r2 = vv_alloc_pages(1);
    memset(r1, 0, sizeof(*r1));
    memset(r2, 0, sizeof(*r2));

    r1->head.type   = VIRTIO_IOMMU_T_MAP;
    r1->domain      = 0;
    r1->virt_start  = 0x30000;
    r1->virt_end    = 0x30FFF;
    r1->phys_start  = 0x80000;
    r1->flags       = VIRTIO_IOMMU_MAP_F_READ;
    r1->tail.status = 0xFF;

    r2->head.type   = VIRTIO_IOMMU_T_UNMAP;
    r2->domain      = 0;
    r2->virt_start  = 0x30000;
    r2->virt_end    = 0x30FFF;
    r2->tail.status = 0xFF;

    uint64_t r1_phys = vv_virt_to_phys(r1);
    uint64_t r2_phys = vv_virt_to_phys(r2);
    size_t   r1_in   = (size_t)((uint8_t *)&r1->tail - (uint8_t *)r1);
    size_t   r2_in   = (size_t)((uint8_t *)&r2->tail - (uint8_t *)r2);

    vring_raw_set_desc(vr, 0, r1_phys, r1_in,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, r1_phys + r1_in, sizeof(r1->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 2, r2_phys, r2_in,
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, r2_phys + r2_in, sizeof(r2->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0034, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_map_then_unmap_same,
              "Map and unmap the same range in one batch",
              VIRTIO_SPEC_V1_2, "5.13.6.3");
