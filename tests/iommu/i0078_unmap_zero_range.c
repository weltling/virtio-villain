/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0078: iommu_unmap_zero_length_range
 *
 * Submit UNMAP with virt_start == virt_end + 1 (zero length).
 * Spec 5.13.6.4 says the device must validate range bounds.
 * A zero length unmap must not corrupt internal page tables
 * or crash the device.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_unmap_zero(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_iommu_req_unmap *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->head.type = VIRTIO_IOMMU_T_UNMAP;
    req->domain = 1;
    req->virt_start = 0x1000;
    req->virt_end = 0x0FFF;  /* end < start => zero length */
    req->tail.status = 0xFF;

    uint64_t base = vv_virt_to_phys(req);
    size_t in_len = (size_t)((uint8_t *)&req->tail - (uint8_t *)req);

    vring_raw_set_desc(vr, 0, base, (uint32_t)in_len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, base + in_len, sizeof(req->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0078, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_unmap_zero,
              "UNMAP with zero length range",
              VIRTIO_SPEC_V1_2, "5.13.6.4");
