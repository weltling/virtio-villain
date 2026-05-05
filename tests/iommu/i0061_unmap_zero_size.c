/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0061: UNMAP with virt_start == virt_end + 1 (zero-size).
 *
 * Spec 5.13.5.7 describes the unmap range as inclusive. A
 * request whose end is one less than start describes a zero-
 * or negative-size range, which is malformed. The device must
 * reject with RANGE.
 *
 * Spec 5.13.5.7.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_unmap_zero_size(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_iommu_req_unmap *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->head.type   = VIRTIO_IOMMU_T_UNMAP;
    req->domain      = 0;
    req->virt_start  = 0x10000;
    req->virt_end    = 0x0FFFF;          /* end < start */
    req->tail.status = 0xFF;

    uint64_t req_phys = vv_virt_to_phys(req);
    size_t   in_len   = (size_t)((uint8_t *)&req->tail - (uint8_t *)req);

    vring_raw_set_desc(vr, 0, req_phys, in_len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, req_phys + in_len, sizeof(req->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0061, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_unmap_zero_size,
              "Unmap with end < start (zero-size range)",
              VIRTIO_SPEC_V1_2, "5.13.5.7");
