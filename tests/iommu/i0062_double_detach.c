/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0062: DETACH twice in a row.
 *
 * After a successful ATTACH, issue DETACH twice. The first
 * DETACH must succeed; the second targets a no-longer-attached
 * endpoint and must either be ignored or rejected with NOENT.
 * The device must not crash.
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

static test_result_t test_iommu_double_detach(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_iommu_req_attach *a  = vv_alloc_pages(1);
    struct virtio_iommu_req_detach *d1 = vv_alloc_pages(1);
    struct virtio_iommu_req_detach *d2 = vv_alloc_pages(1);
    memset(a,  0, sizeof(*a));
    memset(d1, 0, sizeof(*d1));
    memset(d2, 0, sizeof(*d2));

    a->head.type = VIRTIO_IOMMU_T_ATTACH;
    a->domain    = 7;
    a->endpoint  = 0;
    a->tail.status = 0xFF;

    d1->head.type = VIRTIO_IOMMU_T_DETACH;
    d1->domain    = 7;
    d1->endpoint  = 0;
    d1->tail.status = 0xFF;

    *d2 = *d1;

    uint64_t a_p  = vv_virt_to_phys(a);
    uint64_t d1_p = vv_virt_to_phys(d1);
    uint64_t d2_p = vv_virt_to_phys(d2);
    size_t   a_in  = (size_t)((uint8_t *)&a->tail  - (uint8_t *)a);
    size_t   d_in  = (size_t)((uint8_t *)&d1->tail - (uint8_t *)d1);

    vring_raw_set_desc(vr, 0, a_p, a_in, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, a_p + a_in, sizeof(a->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 2, d1_p, d_in, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, d1_p + d_in, sizeof(d1->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 4, d2_p, d_in, VRING_DESC_F_NEXT, 5);
    vring_raw_set_desc(vr, 5, d2_p + d_in, sizeof(d2->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail(vr, 2, 4);
    vring_raw_set_avail_idx(vr, 3);

    return vv_kick_and_wait_n(dev, vr, 0, 3, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0062, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_double_detach,
              "Detach an already-detached endpoint",
              VIRTIO_SPEC_V1_2, "5.13.5.2");
