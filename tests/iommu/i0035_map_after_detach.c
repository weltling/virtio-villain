/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0035: IOMMU MAP after DETACH on a previously attached
 * endpoint.
 *
 * ATTACH endpoint=0 to domain=1, then DETACH that endpoint,
 * then MAP into domain=1. With no endpoint attached the domain
 * still exists (mappings persist) but the device must not crash
 * when honoring the MAP, and the DETACH followed by MAP must
 * not leave dangling references.
 *
 * Spec 5.13.5.1, 5.13.5.2, 5.13.5.6.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_map_after_detach(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_iommu_req_attach *a = vv_alloc_pages(1);
    struct virtio_iommu_req_detach *d = vv_alloc_pages(1);
    struct virtio_iommu_req_map    *m = vv_alloc_pages(1);
    memset(a, 0, sizeof(*a));
    memset(d, 0, sizeof(*d));
    memset(m, 0, sizeof(*m));

    a->head.type = VIRTIO_IOMMU_T_ATTACH;
    a->domain    = 1;
    a->endpoint  = 0;
    a->tail.status = 0xFF;

    d->head.type = VIRTIO_IOMMU_T_DETACH;
    d->domain    = 1;
    d->endpoint  = 0;
    d->tail.status = 0xFF;

    m->head.type   = VIRTIO_IOMMU_T_MAP;
    m->domain      = 1;
    m->virt_start  = 0x40000;
    m->virt_end    = 0x40FFF;
    m->phys_start  = 0x80000;
    m->flags       = VIRTIO_IOMMU_MAP_F_READ;
    m->tail.status = 0xFF;

    uint64_t a_phys = vv_virt_to_phys(a);
    uint64_t d_phys = vv_virt_to_phys(d);
    uint64_t m_phys = vv_virt_to_phys(m);
    size_t   a_in   = (size_t)((uint8_t *)&a->tail - (uint8_t *)a);
    size_t   d_in   = (size_t)((uint8_t *)&d->tail - (uint8_t *)d);
    size_t   m_in   = (size_t)((uint8_t *)&m->tail - (uint8_t *)m);

    vring_raw_set_desc(vr, 0, a_phys, a_in, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, a_phys + a_in, sizeof(a->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 2, d_phys, d_in, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, d_phys + d_in, sizeof(d->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 4, m_phys, m_in, VRING_DESC_F_NEXT, 5);
    vring_raw_set_desc(vr, 5, m_phys + m_in, sizeof(m->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail(vr, 2, 4);
    vring_raw_set_avail_idx(vr, 3);

    return vv_kick_and_wait_n(dev, vr, 0, 3, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0035, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_map_after_detach,
              "Map into domain after detaching its endpoint",
              VIRTIO_SPEC_V1_2, "5.13.5.6");
