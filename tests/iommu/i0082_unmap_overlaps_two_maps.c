/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0082: UNMAP straddling two adjacent MAP regions.
 *
 * Spec 5.13.5.5: UNMAP whose range crosses the boundary between
 * two distinct MAP regions. The device must either tear down both
 * mappings or reject the request, but must not split a single
 * tree node in a way that leaves dangling translations.
 *
 * Sequence: MAP A [0x1000..0x2000), MAP B [0x2000..0x3000),
 * then UNMAP [0x1800..0x2800) which straddles A and B.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t submit_chain(struct virtio_dev *dev, struct vring *vr,
                                  uint8_t *req_page, size_t in_len,
                                  size_t tail_off)
{
    uint64_t req_phys = vv_virt_to_phys(req_page);

    vring_raw_set_desc(vr, 0, req_phys, in_len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, req_phys + tail_off,
                       sizeof(struct virtio_iommu_req_tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

static test_result_t test_iommu_unmap_overlaps_two_maps(struct virtio_dev *dev,
                                                       struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);

    /* MAP A */
    memset(page, 0, sizeof(struct virtio_iommu_req_map));
    struct virtio_iommu_req_map *m = (void *)page;
    m->head.type  = VIRTIO_IOMMU_T_MAP;
    m->domain     = 1;
    m->virt_start = 0x1000;
    m->virt_end   = 0x1FFF;
    m->phys_start = 0x1000;
    m->flags      = VIRTIO_IOMMU_MAP_F_READ | VIRTIO_IOMMU_MAP_F_WRITE;
    m->tail.status = 0xFF;
    size_t in_len_m  = (size_t)((uint8_t *)&m->tail - (uint8_t *)m);
    size_t tail_off_m = in_len_m;

    test_result_t r = submit_chain(dev, vr, page, in_len_m, tail_off_m);
    if (r != TEST_PASS)
        return r;

    /* MAP B */
    vring_raw_set_avail_idx(vr, 0);
    vr->used->idx = 0;
    __sync_synchronize();

    memset(page, 0, sizeof(struct virtio_iommu_req_map));
    m = (void *)page;
    m->head.type  = VIRTIO_IOMMU_T_MAP;
    m->domain     = 1;
    m->virt_start = 0x2000;
    m->virt_end   = 0x2FFF;
    m->phys_start = 0x2000;
    m->flags      = VIRTIO_IOMMU_MAP_F_READ | VIRTIO_IOMMU_MAP_F_WRITE;
    m->tail.status = 0xFF;

    r = submit_chain(dev, vr, page, in_len_m, tail_off_m);
    if (r != TEST_PASS)
        return r;

    /* UNMAP straddling both */
    vring_raw_set_avail_idx(vr, 0);
    vr->used->idx = 0;
    __sync_synchronize();

    memset(page, 0, sizeof(struct virtio_iommu_req_unmap));
    struct virtio_iommu_req_unmap *u = (void *)page;
    u->head.type  = VIRTIO_IOMMU_T_UNMAP;
    u->domain     = 1;
    u->virt_start = 0x1800;
    u->virt_end   = 0x27FF;
    u->tail.status = 0xFF;
    size_t in_len_u   = (size_t)((uint8_t *)&u->tail - (uint8_t *)u);
    size_t tail_off_u = in_len_u;

    return submit_chain(dev, vr, page, in_len_u, tail_off_u);
}

REGISTER_TEST(I0082, VIRTIO_PCI_DEVICE_IOMMU,
              test_iommu_unmap_overlaps_two_maps,
              "UNMAP straddles boundary between two distinct MAPs",
              VIRTIO_SPEC_V1_2, "5.13.5.5");
