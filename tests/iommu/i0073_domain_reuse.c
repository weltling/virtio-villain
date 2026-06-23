/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0073: domain reused by a different endpoint after detach
 *
 * Spec 5.13.5 says ATTACH binds an endpoint to a domain and
 * DETACH releases that binding. After a domain has no attached
 * endpoints, the same domain id must remain usable by another
 * endpoint via a fresh ATTACH. Submit ATTACH endpoint=0 to
 * domain=11, DETACH endpoint=0, ATTACH endpoint=1 to domain=11,
 * DETACH endpoint=1, all in one batch. Every tail status must
 * read OK, proving domain ids survive endpoint churn.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>

static test_result_t test_iommu_domain_reuse(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_iommu_req_attach *a0 = vv_alloc_pages(1);
    struct virtio_iommu_req_detach *d0 = vv_alloc_pages(1);
    struct virtio_iommu_req_attach *a1 = vv_alloc_pages(1);
    struct virtio_iommu_req_detach *d1 = vv_alloc_pages(1);
    memset(a0, 0, sizeof(*a0));
    memset(d0, 0, sizeof(*d0));
    memset(a1, 0, sizeof(*a1));
    memset(d1, 0, sizeof(*d1));

    a0->head.type = VIRTIO_IOMMU_T_ATTACH;
    a0->domain = 11;
    a0->endpoint = 0;
    a0->tail.status = 0xFF;

    d0->head.type = VIRTIO_IOMMU_T_DETACH;
    d0->domain = 11;
    d0->endpoint = 0;
    d0->tail.status = 0xFF;

    a1->head.type = VIRTIO_IOMMU_T_ATTACH;
    a1->domain = 11;
    a1->endpoint = 1;
    a1->tail.status = 0xFF;

    d1->head.type = VIRTIO_IOMMU_T_DETACH;
    d1->domain = 11;
    d1->endpoint = 1;
    d1->tail.status = 0xFF;

    uint64_t a0p = vv_virt_to_phys(a0);
    uint64_t d0p = vv_virt_to_phys(d0);
    uint64_t a1p = vv_virt_to_phys(a1);
    uint64_t d1p = vv_virt_to_phys(d1);
    size_t   ain = (size_t)((uint8_t *)&a0->tail - (uint8_t *)a0);
    size_t   din = (size_t)((uint8_t *)&d0->tail - (uint8_t *)d0);

    vring_raw_set_desc(vr, 0, a0p, ain, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, a0p + ain, sizeof(a0->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 2, d0p, din, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, d0p + din, sizeof(d0->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 4, a1p, ain, VRING_DESC_F_NEXT, 5);
    vring_raw_set_desc(vr, 5, a1p + ain, sizeof(a1->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 6, d1p, din, VRING_DESC_F_NEXT, 7);
    vring_raw_set_desc(vr, 7, d1p + din, sizeof(d1->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail(vr, 2, 4);
    vring_raw_set_avail(vr, 3, 6);
    vring_raw_set_avail_idx(vr, 4);

    test_result_t r = vv_kick_and_wait_n(dev, vr, 0, 4, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    /* The test needs two endpoints bound to devices behind the
     * virtual IOMMU. Only endpoint ids that resolve to a real
     * requester are attachable, the rest return NOENT per spec.
     * If either endpoint is not attachable in this topology there
     * is no cross endpoint domain reuse to exercise, so skip. */
    if (a0->tail.status == VIRTIO_IOMMU_S_NOENT ||
        a1->tail.status == VIRTIO_IOMMU_S_NOENT)
        return TEST_SKIP;

    if (a0->tail.status != 0 || d0->tail.status != 0 ||
        a1->tail.status != 0 || d1->tail.status != 0)
        TFAIL("status a0=%u d0=%u a1=%u d1=%u",
              a0->tail.status, d0->tail.status,
              a1->tail.status, d1->tail.status);

    return TEST_PASS;
}

REGISTER_TEST(I0073, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_domain_reuse,
              "domain id reused by another endpoint after detach",
              VIRTIO_SPEC_V1_2, "5.13.5");
