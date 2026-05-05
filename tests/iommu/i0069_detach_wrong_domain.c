/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0069: DETACH with mismatched domain.
 *
 * ATTACH endpoint to domain X, then DETACH the same endpoint
 * with a different domain id. Per spec 5.13.5.2 the device
 * must verify that domain matches and reject with NOENT or
 * INVAL otherwise.
 *
 * Spec 5.13.5.2.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_detach_wrong_domain(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_iommu_req_attach *a = vv_alloc_pages(1);
    struct virtio_iommu_req_detach *d = vv_alloc_pages(1);
    memset(a, 0, sizeof(*a));
    memset(d, 0, sizeof(*d));

    a->head.type = VIRTIO_IOMMU_T_ATTACH;
    a->domain    = 13;
    a->endpoint  = 0;
    a->tail.status = 0xFF;

    d->head.type = VIRTIO_IOMMU_T_DETACH;
    d->domain    = 14;                   /* wrong */
    d->endpoint  = 0;
    d->tail.status = 0xFF;

    uint64_t a_p = vv_virt_to_phys(a);
    uint64_t d_p = vv_virt_to_phys(d);
    size_t   a_in = (size_t)((uint8_t *)&a->tail - (uint8_t *)a);
    size_t   d_in = (size_t)((uint8_t *)&d->tail - (uint8_t *)d);

    vring_raw_set_desc(vr, 0, a_p, a_in, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, a_p + a_in, sizeof(a->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 2, d_p, d_in, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, d_p + d_in, sizeof(d->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0069, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_detach_wrong_domain,
              "Detach with mismatched domain id",
              VIRTIO_SPEC_V1_2, "5.13.5.2");
