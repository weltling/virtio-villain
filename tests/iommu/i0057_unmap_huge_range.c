/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0057: UNMAP a single huge range covering many small MAPs.
 *
 * Create three small page-sized mappings inside one larger
 * virtual window, then UNMAP the whole window in one request.
 * Per spec 5.13.5.7 all three mappings must be removed in
 * a single operation.
 *
 * Spec 5.13.5.7.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_unmap_huge_range(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_iommu_req_map   *m1 = vv_alloc_pages(1);
    struct virtio_iommu_req_map   *m2 = vv_alloc_pages(1);
    struct virtio_iommu_req_map   *m3 = vv_alloc_pages(1);
    struct virtio_iommu_req_unmap *u  = vv_alloc_pages(1);
    memset(m1, 0, sizeof(*m1));
    memset(m2, 0, sizeof(*m2));
    memset(m3, 0, sizeof(*m3));
    memset(u,  0, sizeof(*u));

    m1->head.type   = VIRTIO_IOMMU_T_MAP;
    m1->domain      = 0;
    m1->virt_start  = 0xE0000;
    m1->virt_end    = 0xE0FFF;
    m1->phys_start  = 0x80000;
    m1->flags       = VIRTIO_IOMMU_MAP_F_READ;
    m1->tail.status = 0xFF;

    *m2 = *m1;
    m2->virt_start = 0xE2000;
    m2->virt_end   = 0xE2FFF;
    m2->phys_start = 0x82000;

    *m3 = *m1;
    m3->virt_start = 0xE4000;
    m3->virt_end   = 0xE4FFF;
    m3->phys_start = 0x84000;

    u->head.type   = VIRTIO_IOMMU_T_UNMAP;
    u->domain      = 0;
    u->virt_start  = 0xE0000;
    u->virt_end    = 0xEFFFF;
    u->tail.status = 0xFF;

    uint64_t m1p = vv_virt_to_phys(m1);
    uint64_t m2p = vv_virt_to_phys(m2);
    uint64_t m3p = vv_virt_to_phys(m3);
    uint64_t up  = vv_virt_to_phys(u);
    size_t   m_in = (size_t)((uint8_t *)&m1->tail - (uint8_t *)m1);
    size_t   u_in = (size_t)((uint8_t *)&u->tail  - (uint8_t *)u);

    vring_raw_set_desc(vr, 0, m1p, m_in, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, m1p + m_in, sizeof(m1->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 2, m2p, m_in, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, m2p + m_in, sizeof(m2->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 4, m3p, m_in, VRING_DESC_F_NEXT, 5);
    vring_raw_set_desc(vr, 5, m3p + m_in, sizeof(m3->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 6, up, u_in, VRING_DESC_F_NEXT, 7);
    vring_raw_set_desc(vr, 7, up + u_in, sizeof(u->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail(vr, 2, 4);
    vring_raw_set_avail(vr, 3, 6);
    vring_raw_set_avail_idx(vr, 4);

    return vv_kick_and_wait_n(dev, vr, 0, 4, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0057, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_unmap_huge_range,
              "Unmap one large range covering many mappings",
              VIRTIO_SPEC_V1_2, "5.13.5.7");
