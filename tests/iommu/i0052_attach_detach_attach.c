/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0052: ATTACH, DETACH, ATTACH cycle on a single endpoint.
 *
 * Three sequential operations on the same endpoint exercise
 * the device's reference counting between ATTACH and DETACH.
 * All three must succeed.
 *
 * Spec 5.13.5.1, 5.13.5.2.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_attach_detach_attach(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    struct virtio_iommu_req_attach *a1 = vv_alloc_pages(1);
    struct virtio_iommu_req_detach *d  = vv_alloc_pages(1);
    struct virtio_iommu_req_attach *a2 = vv_alloc_pages(1);
    memset(a1, 0, sizeof(*a1));
    memset(d,  0, sizeof(*d));
    memset(a2, 0, sizeof(*a2));

    a1->head.type = VIRTIO_IOMMU_T_ATTACH;
    a1->domain    = 5;
    a1->endpoint  = 0;
    a1->tail.status = 0xFF;

    d->head.type = VIRTIO_IOMMU_T_DETACH;
    d->domain    = 5;
    d->endpoint  = 0;
    d->tail.status = 0xFF;

    a2->head.type = VIRTIO_IOMMU_T_ATTACH;
    a2->domain    = 5;
    a2->endpoint  = 0;
    a2->tail.status = 0xFF;

    uint64_t a1_p = vv_virt_to_phys(a1);
    uint64_t d_p  = vv_virt_to_phys(d);
    uint64_t a2_p = vv_virt_to_phys(a2);
    size_t   a1_in = (size_t)((uint8_t *)&a1->tail - (uint8_t *)a1);
    size_t   d_in  = (size_t)((uint8_t *)&d->tail  - (uint8_t *)d);
    size_t   a2_in = (size_t)((uint8_t *)&a2->tail - (uint8_t *)a2);

    vring_raw_set_desc(vr, 0, a1_p, a1_in, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, a1_p + a1_in, sizeof(a1->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 2, d_p, d_in, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, d_p + d_in, sizeof(d->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 4, a2_p, a2_in, VRING_DESC_F_NEXT, 5);
    vring_raw_set_desc(vr, 5, a2_p + a2_in, sizeof(a2->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail(vr, 2, 4);
    vring_raw_set_avail_idx(vr, 3);

    return vv_kick_and_wait_n(dev, vr, 0, 3, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0052, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_attach_detach_attach,
              "Attach, detach, attach again on same endpoint",
              VIRTIO_SPEC_V1_2, "5.13.5.1");
