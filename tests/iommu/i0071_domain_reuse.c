/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0071: domain reused by second endpoint after detach
 *
 * Spec 5.13.5 allows a domain to be shared between endpoints. Run
 * the sequence ATTACH(ep0) MAP UNMAP DETACH(ep0) ATTACH(ep1) MAP
 * UNMAP DETACH(ep1) all targeting the same domain id. Each step
 * after the first detach exercises domain reuse semantics where a
 * VMM that ties domain state to the first endpoint will trip up.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static int submit_pair(struct vring *vr, uint16_t slot, uint64_t req_phys,
                       size_t in_len, size_t tail_off, uint16_t next_used)
{
    vring_raw_set_desc(vr, slot, req_phys, in_len, VRING_DESC_F_NEXT,
                       (uint16_t)(slot + 1));
    vring_raw_set_desc(vr, (uint16_t)(slot + 1), req_phys + tail_off,
                       (uint32_t)(in_len + 4 - tail_off),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, next_used, slot);
    return 0;
}

static test_result_t test_iommu_domain_reuse(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_iommu_req_attach *a0 = vv_alloc_pages(1);
    struct virtio_iommu_req_map    *m0 = vv_alloc_pages(1);
    struct virtio_iommu_req_unmap  *u0 = vv_alloc_pages(1);
    struct virtio_iommu_req_detach *d0 = vv_alloc_pages(1);
    struct virtio_iommu_req_attach *a1 = vv_alloc_pages(1);
    struct virtio_iommu_req_map    *m1 = vv_alloc_pages(1);
    struct virtio_iommu_req_unmap  *u1 = vv_alloc_pages(1);
    struct virtio_iommu_req_detach *d1 = vv_alloc_pages(1);
    memset(a0, 0, sizeof(*a0)); memset(m0, 0, sizeof(*m0));
    memset(u0, 0, sizeof(*u0)); memset(d0, 0, sizeof(*d0));
    memset(a1, 0, sizeof(*a1)); memset(m1, 0, sizeof(*m1));
    memset(u1, 0, sizeof(*u1)); memset(d1, 0, sizeof(*d1));

    const uint32_t dom = 9;

    a0->head.type = VIRTIO_IOMMU_T_ATTACH;
    a0->domain = dom; a0->endpoint = 0; a0->tail.status = 0xFF;
    m0->head.type = VIRTIO_IOMMU_T_MAP;
    m0->domain = dom; m0->virt_start = 0x80000; m0->virt_end = 0x80FFF;
    m0->phys_start = 0x100000;
    m0->flags = VIRTIO_IOMMU_MAP_F_READ | VIRTIO_IOMMU_MAP_F_WRITE;
    m0->tail.status = 0xFF;
    u0->head.type = VIRTIO_IOMMU_T_UNMAP;
    u0->domain = dom; u0->virt_start = 0x80000; u0->virt_end = 0x80FFF;
    u0->tail.status = 0xFF;
    d0->head.type = VIRTIO_IOMMU_T_DETACH;
    d0->domain = dom; d0->endpoint = 0; d0->tail.status = 0xFF;

    a1->head.type = VIRTIO_IOMMU_T_ATTACH;
    a1->domain = dom; a1->endpoint = 1; a1->tail.status = 0xFF;
    m1->head.type = VIRTIO_IOMMU_T_MAP;
    m1->domain = dom; m1->virt_start = 0x90000; m1->virt_end = 0x90FFF;
    m1->phys_start = 0x110000;
    m1->flags = VIRTIO_IOMMU_MAP_F_READ | VIRTIO_IOMMU_MAP_F_WRITE;
    m1->tail.status = 0xFF;
    u1->head.type = VIRTIO_IOMMU_T_UNMAP;
    u1->domain = dom; u1->virt_start = 0x90000; u1->virt_end = 0x90FFF;
    u1->tail.status = 0xFF;
    d1->head.type = VIRTIO_IOMMU_T_DETACH;
    d1->domain = dom; d1->endpoint = 1; d1->tail.status = 0xFF;

    struct {
        void *p;
        size_t in;
    } steps[8] = {
        { a0, (size_t)((uint8_t *)&a0->tail - (uint8_t *)a0) },
        { m0, (size_t)((uint8_t *)&m0->tail - (uint8_t *)m0) },
        { u0, (size_t)((uint8_t *)&u0->tail - (uint8_t *)u0) },
        { d0, (size_t)((uint8_t *)&d0->tail - (uint8_t *)d0) },
        { a1, (size_t)((uint8_t *)&a1->tail - (uint8_t *)a1) },
        { m1, (size_t)((uint8_t *)&m1->tail - (uint8_t *)m1) },
        { u1, (size_t)((uint8_t *)&u1->tail - (uint8_t *)u1) },
        { d1, (size_t)((uint8_t *)&d1->tail - (uint8_t *)d1) },
    };

    if (vr->size < 16)
        return TEST_SKIP;

    for (int i = 0; i < 8; i++) {
        uint64_t phys = vv_virt_to_phys(steps[i].p);
        submit_pair(vr, (uint16_t)(i * 2), phys, steps[i].in,
                    steps[i].in, (uint16_t)i);
    }
    vring_raw_set_avail_idx(vr, 8);

    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if (vr->used->idx >= 8)
            return TEST_PASS;
        elapsed += 10000;
    }
    TREJECT("no device response within timeout");
}

REGISTER_TEST(I0071, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_domain_reuse,
              "Same domain shared between two endpoints sequentially",
              VIRTIO_SPEC_V1_2, "5.13.5");
