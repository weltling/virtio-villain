/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0047: IOMMU full ATTACH/MAP/UNMAP/DETACH life cycle.
 *
 * Submit a complete sequence: ATTACH, MAP, UNMAP, DETACH all
 * referencing the same domain and endpoint, in a single batch.
 * Spec 5.13.5 describes the canonical life cycle; the device
 * must process all four requests successfully and leave the
 * endpoint detached at the end.
 *
 * Spec 5.13.5.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_full_lifecycle(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_iommu_req_attach *a = vv_alloc_pages(1);
    struct virtio_iommu_req_map    *m = vv_alloc_pages(1);
    struct virtio_iommu_req_unmap  *u = vv_alloc_pages(1);
    struct virtio_iommu_req_detach *d = vv_alloc_pages(1);
    memset(a, 0, sizeof(*a));
    memset(m, 0, sizeof(*m));
    memset(u, 0, sizeof(*u));
    memset(d, 0, sizeof(*d));

    a->head.type = VIRTIO_IOMMU_T_ATTACH;
    a->domain    = 1;
    a->endpoint  = 0;
    a->tail.status = 0xFF;

    m->head.type   = VIRTIO_IOMMU_T_MAP;
    m->domain      = 1;
    m->virt_start  = 0x50000;
    m->virt_end    = 0x50FFF;
    m->phys_start  = 0x80000;
    m->flags       = VIRTIO_IOMMU_MAP_F_READ | VIRTIO_IOMMU_MAP_F_WRITE;
    m->tail.status = 0xFF;

    u->head.type   = VIRTIO_IOMMU_T_UNMAP;
    u->domain      = 1;
    u->virt_start  = 0x50000;
    u->virt_end    = 0x50FFF;
    u->tail.status = 0xFF;

    d->head.type = VIRTIO_IOMMU_T_DETACH;
    d->domain    = 1;
    d->endpoint  = 0;
    d->tail.status = 0xFF;

    uint64_t a_phys = vv_virt_to_phys(a);
    uint64_t m_phys = vv_virt_to_phys(m);
    uint64_t u_phys = vv_virt_to_phys(u);
    uint64_t d_phys = vv_virt_to_phys(d);
    size_t   a_in   = (size_t)((uint8_t *)&a->tail - (uint8_t *)a);
    size_t   m_in   = (size_t)((uint8_t *)&m->tail - (uint8_t *)m);
    size_t   u_in   = (size_t)((uint8_t *)&u->tail - (uint8_t *)u);
    size_t   d_in   = (size_t)((uint8_t *)&d->tail - (uint8_t *)d);

    vring_raw_set_desc(vr, 0, a_phys, a_in, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, a_phys + a_in, sizeof(a->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 2, m_phys, m_in, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, m_phys + m_in, sizeof(m->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 4, u_phys, u_in, VRING_DESC_F_NEXT, 5);
    vring_raw_set_desc(vr, 5, u_phys + u_in, sizeof(u->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 6, d_phys, d_in, VRING_DESC_F_NEXT, 7);
    vring_raw_set_desc(vr, 7, d_phys + d_in, sizeof(d->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail(vr, 2, 4);
    vring_raw_set_avail(vr, 3, 6);
    vring_raw_set_avail_idx(vr, 4);

    return vv_kick_and_wait_n(dev, vr, 0, 4, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0047, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_full_lifecycle,
              "Attach, map, unmap, detach in one batch",
              VIRTIO_SPEC_V1_2, "5.13.5");
