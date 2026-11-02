/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0108: IOMMU attach verifies S_OK status explicitly.
 *
 * Submit a single ATTACH request and verify the tail status byte
 * is exactly 0 (S_OK). Prior attach tests often batch with MAP
 * or DETACH; this isolates the attach response.
 */
#include "lib/virtio_test.h"
#include "lib/virtio_iommu.h"

static test_result_t test_iommu_attach_verify(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_iommu_req_attach *a = vv_alloc_pages(1);

    a->head.type = VIRTIO_IOMMU_T_ATTACH;
    a->domain = 50;
    a->endpoint = 0;
    a->tail.status = 0xFF;

    vv_desc(vr, 0, a, sizeof(*a) - sizeof(a->tail), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, &a->tail, sizeof(a->tail), VRING_DESC_F_WRITE, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r != TEST_PASS) return r;

    if (a->tail.status != 0)
        TFAIL("attach status %u, expected 0 (S_OK)", a->tail.status);

    return TEST_PASS;
}

REGISTER_TEST(I0108, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_attach_verify,
              "Single ATTACH returns S_OK",
              VIRTIO_SPEC_V1_2, "5.13.6.2");
