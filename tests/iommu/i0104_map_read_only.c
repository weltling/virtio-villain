/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0104: IOMMU MAP with read only flag.
 *
 * Spec 5.13.6.6: MAP with VIRTIO_IOMMU_MAP_F_READ only (no WRITE).
 * The device must accept the mapping. This tests that the device
 * handles the read only permission flag correctly rather than
 * requiring both READ and WRITE.
 */
#include "lib/virtio_test.h"
#include "lib/virtio_iommu.h"

static test_result_t test_iommu_map_ro(struct virtio_dev *dev,
                                       struct vring *vr)
{
    struct virtio_iommu_req_attach *a = vv_alloc_pages(1);
    struct virtio_iommu_req_map *m = vv_alloc_pages(1);
    struct virtio_iommu_req_unmap *u = vv_alloc_pages(1);
    struct virtio_iommu_req_detach *d = vv_alloc_pages(1);

    /* ATTACH */
    a->head.type = VIRTIO_IOMMU_T_ATTACH;
    a->domain = 10;
    a->endpoint = 0;
    a->tail.status = 0xFF;

    /* MAP read only */
    void *backing = vv_alloc_pages(1);
    m->head.type = VIRTIO_IOMMU_T_MAP;
    m->domain = 10;
    m->virt_start = 0xB0000;
    m->virt_end = 0xB0FFF;
    m->phys_start = vv_virt_to_phys(backing);
    m->flags = VIRTIO_IOMMU_MAP_F_READ;  /* read only, no WRITE */
    m->tail.status = 0xFF;

    /* UNMAP */
    u->head.type = VIRTIO_IOMMU_T_UNMAP;
    u->domain = 10;
    u->virt_start = 0xB0000;
    u->virt_end = 0xB0FFF;
    u->tail.status = 0xFF;

    /* DETACH */
    d->head.type = VIRTIO_IOMMU_T_DETACH;
    d->domain = 10;
    d->endpoint = 0;
    d->tail.status = 0xFF;

    /* Submit all four in sequence */
    vv_desc(vr, 0, a, sizeof(*a) - sizeof(a->tail), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, &a->tail, sizeof(a->tail), VRING_DESC_F_WRITE, 0);

    vv_desc(vr, 2, m, sizeof(*m) - sizeof(m->tail), VRING_DESC_F_NEXT, 3);
    vv_desc(vr, 3, &m->tail, sizeof(m->tail), VRING_DESC_F_WRITE, 0);

    vv_desc(vr, 4, u, sizeof(*u) - sizeof(u->tail), VRING_DESC_F_NEXT, 5);
    vv_desc(vr, 5, &u->tail, sizeof(u->tail), VRING_DESC_F_WRITE, 0);

    vv_desc(vr, 6, d, sizeof(*d) - sizeof(d->tail), VRING_DESC_F_NEXT, 7);
    vv_desc(vr, 7, &d->tail, sizeof(d->tail), VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail(vr, 2, 4);
    vring_raw_set_avail(vr, 3, 6);
    vring_raw_set_avail_idx(vr, 4);

    test_result_t r = vv_kick_and_wait_n(dev, vr, 0, 4, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    if (a->tail.status != 0) TFAIL("attach status %u", a->tail.status);
    if (m->tail.status != 0) TFAIL("map status %u", m->tail.status);
    if (u->tail.status != 0) TFAIL("unmap status %u", u->tail.status);
    if (d->tail.status != 0) TFAIL("detach status %u", d->tail.status);

    return TEST_PASS;
}

REGISTER_TEST(I0104, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_map_ro,
              "MAP with read only flag then unmap",
              VIRTIO_SPEC_V1_2, "5.13.6.6");
