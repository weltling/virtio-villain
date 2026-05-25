/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0084: IOMMU MAP at the top of the 64 bit virtual address space.
 *
 * Spec 5.13.5.3: virt_start and virt_end specify a closed range
 * of virtual addresses to map. Submit a single page mapping at
 * [0xFFFFFFFFFFFFE000, 0xFFFFFFFFFFFFEFFF]. The device must
 * accept the boundary range or reject cleanly; it must not
 * overflow when computing range length and must not corrupt
 * the radix tree at the very top of the virt address space.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_map_top_of_address_space(struct virtio_dev *dev,
                                                         struct vring *vr)
{
    struct virtio_iommu_req_attach *att = vv_alloc_pages(1);
    memset(att, 0, sizeof(*att));
    att->head.type = VIRTIO_IOMMU_T_ATTACH;
    att->domain    = 1;
    att->endpoint  = 0;
    att->tail.status = 0xFF;

    size_t att_in = (size_t)((uint8_t *)&att->tail - (uint8_t *)att);

    struct virtio_iommu_req_map *m = vv_alloc_pages(1);
    memset(m, 0, sizeof(*m));
    m->head.type   = VIRTIO_IOMMU_T_MAP;
    m->domain      = 1;
    m->virt_start  = 0xFFFFFFFFFFFFE000ULL;
    m->virt_end    = 0xFFFFFFFFFFFFEFFFULL;
    m->phys_start  = 0x100000;
    m->flags       = VIRTIO_IOMMU_MAP_F_READ | VIRTIO_IOMMU_MAP_F_WRITE;
    m->tail.status = 0xFF;

    size_t m_in = (size_t)((uint8_t *)&m->tail - (uint8_t *)m);

    uint64_t att_phys = vv_virt_to_phys(att);
    uint64_t m_phys   = vv_virt_to_phys(m);

    vring_raw_set_desc(vr, 0, att_phys, att_in,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, att_phys + att_in, sizeof(att->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 2, m_phys, m_in,
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, m_phys + m_in, sizeof(m->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0084, VIRTIO_PCI_DEVICE_IOMMU,
              test_iommu_map_top_of_address_space,
              "MAP single page at top of 64 bit virtual address space",
              VIRTIO_SPEC_V1_2, "5.13.5.3");
