/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0075: iommu_probe_huge_properties
 *
 * Issue PROBE with a properties area length close to 4 GiB by
 * stretching the descriptor length. Spec 5.13.6.6 caps probe
 * output to a sane value. The host must perform the bounds check
 * without integer overflow and must not panic.
 */
#include "lib/virtio_test.h"
#include "lib/virtio_iommu.h"

static test_result_t test_iommu_probe_huge(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_iommu_req_probe *req = vv_alloc_pages(1);

    req->head.type = VIRTIO_IOMMU_T_PROBE;
    req->endpoint = 0;
    req->tail.status = 0xFF;

    /* in_len is the request header through endpoint plus reserved */
    size_t in_len = sizeof(req->head) + sizeof(req->endpoint)
                    + sizeof(req->reserved);
    uint64_t out_phys;
    vv_alloc_page_high(&out_phys);

    vv_desc(vr, 0, req, (uint32_t)in_len, VRING_DESC_F_NEXT, 1);
    /* Output buffer with a comically large length */
    vring_raw_set_desc(vr, 1, out_phys, 0xFFFFF000,
                       VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(I0075, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_probe_huge,
              "PROBE with 4 GiB output descriptor length",
              VIRTIO_SPEC_V1_2, "5.13.6.6");
