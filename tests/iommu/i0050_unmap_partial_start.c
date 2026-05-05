/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0050: UNMAP straddling the start of an existing mapping.
 *
 * MAP a range, then UNMAP a range whose virt_start lies before
 * the mapping and whose virt_end lies inside it. Spec 5.13.5.7
 * requires UNMAP to either remove only mappings strictly
 * contained in the range or reject with RANGE. The device must
 * not crash and must not leave half-mappings behind.
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

static test_result_t test_iommu_unmap_partial_start(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_iommu_req_map   *m = vv_alloc_pages(1);
    struct virtio_iommu_req_unmap *u = vv_alloc_pages(1);
    memset(m, 0, sizeof(*m));
    memset(u, 0, sizeof(*u));

    m->head.type   = VIRTIO_IOMMU_T_MAP;
    m->domain      = 0;
    m->virt_start  = 0xA0000;
    m->virt_end    = 0xA1FFF;
    m->phys_start  = 0x80000;
    m->flags       = VIRTIO_IOMMU_MAP_F_READ;
    m->tail.status = 0xFF;

    u->head.type   = VIRTIO_IOMMU_T_UNMAP;
    u->domain      = 0;
    u->virt_start  = 0x9F000;            /* before mapping */
    u->virt_end    = 0xA0FFF;            /* inside mapping */
    u->tail.status = 0xFF;

    uint64_t m_phys = vv_virt_to_phys(m);
    uint64_t u_phys = vv_virt_to_phys(u);
    size_t   m_in   = (size_t)((uint8_t *)&m->tail - (uint8_t *)m);
    size_t   u_in   = (size_t)((uint8_t *)&u->tail - (uint8_t *)u);

    vring_raw_set_desc(vr, 0, m_phys, m_in, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, m_phys + m_in, sizeof(m->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 2, u_phys, u_in, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, u_phys + u_in, sizeof(u->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0050, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_unmap_partial_start,
              "Unmap straddling the start of a mapping",
              VIRTIO_SPEC_V1_2, "5.13.5.7");
