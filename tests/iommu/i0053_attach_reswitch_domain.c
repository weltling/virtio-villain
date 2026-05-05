/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0053: Re-attach an already attached endpoint to a different
 * domain without DETACH first.
 *
 * Per spec 5.13.5.1 the device behavior in this case is left to
 * the implementation: it may either implicitly detach from the
 * old domain and attach to the new one, or reject with UNSUPP.
 * The device must not crash.
 *
 * Spec 5.13.5.1.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_reattach_other_domain(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    struct virtio_iommu_req_attach *a1 = vv_alloc_pages(1);
    struct virtio_iommu_req_attach *a2 = vv_alloc_pages(1);
    memset(a1, 0, sizeof(*a1));
    memset(a2, 0, sizeof(*a2));

    a1->head.type = VIRTIO_IOMMU_T_ATTACH;
    a1->domain    = 10;
    a1->endpoint  = 0;
    a1->tail.status = 0xFF;

    a2->head.type = VIRTIO_IOMMU_T_ATTACH;
    a2->domain    = 11;
    a2->endpoint  = 0;
    a2->tail.status = 0xFF;

    uint64_t a1_p = vv_virt_to_phys(a1);
    uint64_t a2_p = vv_virt_to_phys(a2);
    size_t   a_in = (size_t)((uint8_t *)&a1->tail - (uint8_t *)a1);

    vring_raw_set_desc(vr, 0, a1_p, a_in, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, a1_p + a_in, sizeof(a1->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 2, a2_p, a_in, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, a2_p + a_in, sizeof(a2->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0053, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_reattach_other_domain,
              "Re-attach endpoint to a different domain",
              VIRTIO_SPEC_V1_2, "5.13.5.1");
