/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0049: IOMMU multiple non-overlapping MAPs in one batch.
 *
 * Submit two MAPs in the same domain at non-overlapping
 * virtual ranges. Both must succeed. Stresses the device
 * range table on the happy path.
 *
 * Spec 5.13.5.6.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_two_maps(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_iommu_req_map *r1 = vv_alloc_pages(1);
    struct virtio_iommu_req_map *r2 = vv_alloc_pages(1);
    memset(r1, 0, sizeof(*r1));
    memset(r2, 0, sizeof(*r2));

    r1->head.type   = VIRTIO_IOMMU_T_MAP;
    r1->domain      = 0;
    r1->virt_start  = 0x70000;
    r1->virt_end    = 0x70FFF;
    r1->phys_start  = 0x80000;
    r1->flags       = VIRTIO_IOMMU_MAP_F_READ;
    r1->tail.status = 0xFF;

    *r2 = *r1;
    r2->virt_start = 0x71000;
    r2->virt_end   = 0x71FFF;
    r2->phys_start = 0x81000;

    uint64_t r1_phys = vv_virt_to_phys(r1);
    uint64_t r2_phys = vv_virt_to_phys(r2);
    size_t   in_len  = (size_t)((uint8_t *)&r1->tail - (uint8_t *)r1);

    vring_raw_set_desc(vr, 0, r1_phys, in_len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, r1_phys + in_len, sizeof(r1->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 2, r2_phys, in_len,
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, r2_phys + in_len, sizeof(r2->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0049, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_two_maps,
              "Two non-overlapping MAPs succeed",
              VIRTIO_SPEC_V1_2, "5.13.5.6");
