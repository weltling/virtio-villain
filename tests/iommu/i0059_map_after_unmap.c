/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0059: MAP, UNMAP, MAP same range with different phys_start.
 *
 * The first MAP installs a translation, UNMAP removes it, the
 * second MAP installs a fresh translation at the same virtual
 * range pointing to a different physical address. Spec 5.13.5
 * says the second MAP must succeed because the range is empty
 * after UNMAP.
 *
 * Spec 5.13.5.6, 5.13.5.7.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_map_after_unmap(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_iommu_req_map   *m1 = vv_alloc_pages(1);
    struct virtio_iommu_req_unmap *u  = vv_alloc_pages(1);
    struct virtio_iommu_req_map   *m2 = vv_alloc_pages(1);
    memset(m1, 0, sizeof(*m1));
    memset(u,  0, sizeof(*u));
    memset(m2, 0, sizeof(*m2));

    m1->head.type   = VIRTIO_IOMMU_T_MAP;
    m1->domain      = 0;
    m1->virt_start  = 0xF0000;
    m1->virt_end    = 0xF0FFF;
    m1->phys_start  = 0x80000;
    m1->flags       = VIRTIO_IOMMU_MAP_F_READ;
    m1->tail.status = 0xFF;

    u->head.type   = VIRTIO_IOMMU_T_UNMAP;
    u->domain      = 0;
    u->virt_start  = 0xF0000;
    u->virt_end    = 0xF0FFF;
    u->tail.status = 0xFF;

    *m2 = *m1;
    m2->phys_start = 0x90000;

    uint64_t m1p = vv_virt_to_phys(m1);
    uint64_t up  = vv_virt_to_phys(u);
    uint64_t m2p = vv_virt_to_phys(m2);
    size_t   m_in = (size_t)((uint8_t *)&m1->tail - (uint8_t *)m1);
    size_t   u_in = (size_t)((uint8_t *)&u->tail  - (uint8_t *)u);

    vring_raw_set_desc(vr, 0, m1p, m_in, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, m1p + m_in, sizeof(m1->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 2, up, u_in, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, up + u_in, sizeof(u->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 4, m2p, m_in, VRING_DESC_F_NEXT, 5);
    vring_raw_set_desc(vr, 5, m2p + m_in, sizeof(m2->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail(vr, 2, 4);
    vring_raw_set_avail_idx(vr, 3);

    return vv_kick_and_wait_n(dev, vr, 0, 3, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0059, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_map_after_unmap,
              "Map after unmap with new phys_start",
              VIRTIO_SPEC_V1_2, "5.13.5.6");
