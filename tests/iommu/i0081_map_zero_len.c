/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0081: IOMMU MAP with zero length range.
 *
 * Spec 5.13.6.4: Submit a MAP request with virt_start == virt_end
 * (zero length). The device must reject the degenerate mapping
 * without corrupting internal page tables.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_map_zero_len(struct virtio_dev *dev,
                                             struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);
    memset(page, 0, 4096);

    struct virtio_iommu_req_map *req = (void *)page;
    req->head.type  = VIRTIO_IOMMU_T_MAP;
    req->domain     = 1;
    req->virt_start = 0x1000;
    req->virt_end   = 0x1000; /* zero length: start == end */
    req->phys_start = 0x1000;
    req->flags      = 0x3; /* read + write */
    req->tail.status = 0xFF;

    uint64_t phys = vv_virt_to_phys(page);
    size_t req_len = sizeof(*req) - sizeof(req->tail);
    size_t tail_off = (size_t)((uint8_t *)&req->tail - (uint8_t *)req);

    vring_raw_set_desc(vr, 0, phys, req_len, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, phys + tail_off, sizeof(req->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0081, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_map_zero_len,
              "IOMMU MAP with zero length range",
              VIRTIO_SPEC_V1_2, "5.13.6.4");
