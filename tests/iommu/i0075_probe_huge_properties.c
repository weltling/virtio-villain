/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0075: iommu_probe_huge_properties
 *
 * Issue PROBE with a properties area length close to 4 GiB by
 * stretching the descriptor length. Spec 5.13.6.6 caps probe
 * output to a sane value. The host must perform the bounds check
 * without integer overflow and must not panic.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_probe_huge(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_iommu_req_probe *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->head.type = VIRTIO_IOMMU_T_PROBE;
    req->endpoint = 0;
    req->tail.status = 0xFF;

    uint64_t base = vv_virt_to_phys(req);
    /* in_len is the request header through endpoint plus reserved */
    size_t in_len = sizeof(req->head) + sizeof(req->endpoint)
                    + sizeof(req->reserved);
    uint64_t out_phys;
    vv_alloc_page_high(&out_phys);

    vring_raw_set_desc(vr, 0, base, (uint32_t)in_len,
                       VRING_DESC_F_NEXT, 1);
    /* Output buffer with a comically large length */
    vring_raw_set_desc(vr, 1, out_phys, 0xFFFFF000,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0075, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_probe_huge,
              "PROBE with 4 GiB output descriptor length",
              VIRTIO_SPEC_V1_2, "5.13.6.6");
