/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0019: IOMMU re-attach an endpoint to a different domain
 * without first detaching.
 *
 * Submit ATTACH(endpoint=0, domain=1) then ATTACH(endpoint=0,
 * domain=2). Spec 5.13.5.1 says the driver MUST detach an
 * endpoint before attaching it to a different domain. The device
 * must reject the second attach with VIRTIO_IOMMU_S_INVAL.
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

static test_result_t test_iommu_attach_twice(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_iommu_req_attach *r1 = vv_alloc_pages(1);
    struct virtio_iommu_req_attach *r2 = vv_alloc_pages(1);
    memset(r1, 0, sizeof(*r1));
    memset(r2, 0, sizeof(*r2));

    r1->head.type = VIRTIO_IOMMU_T_ATTACH;
    r1->domain    = 1;
    r1->endpoint  = 0;
    r1->tail.status = 0xFF;

    *r2 = *r1;
    r2->domain = 2;

    uint64_t r1_phys = vv_virt_to_phys(r1);
    uint64_t r2_phys = vv_virt_to_phys(r2);
    size_t   in_len  = (size_t)((uint8_t *)&r1->tail - (uint8_t *)r1);

    vring_raw_set_desc(vr, 0, r1_phys, in_len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, r1_phys + in_len, sizeof(r1->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 2, r2_phys, in_len,
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, r2_phys + in_len, sizeof(r2->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0019, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_attach_twice,
              "Re-attach endpoint without prior detach",
              VIRTIO_SPEC_V1_2, "5.13.5.1");
