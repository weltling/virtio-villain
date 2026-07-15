/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0106: IOMMU detach returns S_OK.
 *
 * Spec 5.13.6.3: DETACH removes the endpoint from its domain.
 * Submit ATTACH then DETACH and verify both return status 0.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_detach_ok(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_iommu_req_attach *a = vv_alloc_pages(1);
    struct virtio_iommu_req_detach *d = vv_alloc_pages(1);

    memset(a, 0, sizeof(*a));
    memset(d, 0, sizeof(*d));

    a->head.type = VIRTIO_IOMMU_T_ATTACH;
    a->domain = 30;
    a->endpoint = 0;
    a->tail.status = 0xFF;

    d->head.type = VIRTIO_IOMMU_T_DETACH;
    d->domain = 30;
    d->endpoint = 0;
    d->tail.status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(a),
                       sizeof(*a) - sizeof(a->tail),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(&a->tail),
                       sizeof(a->tail), VRING_DESC_F_WRITE, 0);

    vring_raw_set_desc(vr, 2, vv_virt_to_phys(d),
                       sizeof(*d) - sizeof(d->tail),
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(&d->tail),
                       sizeof(d->tail), VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    test_result_t r = vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    if (a->tail.status != 0) TFAIL("attach status %u", a->tail.status);
    if (d->tail.status != 0) TFAIL("detach status %u", d->tail.status);

    return TEST_PASS;
}

REGISTER_TEST(I0106, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_detach_ok,
              "Attach then detach both return S_OK",
              VIRTIO_SPEC_V1_2, "5.13.6.3");
