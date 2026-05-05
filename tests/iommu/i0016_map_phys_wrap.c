/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0016: IOMMU MAP with phys_start that wraps around 64 bits.
 *
 * Set phys_start near UINT64_MAX so that phys_start + size wraps
 * back to a low address. The device must detect the wrap and
 * reject the request, never trust the wrapped result for further
 * arithmetic.
 *
 * Spec 5.13.6.3.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_map_phys_wrap(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_iommu_req_map *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->head.type   = VIRTIO_IOMMU_T_MAP;
    req->domain      = 0;
    req->virt_start  = 0x1000;
    req->virt_end    = 0x1FFF;
    req->phys_start  = (uint64_t)-0x800;  /* 2KiB before wrap */
    req->flags       = VIRTIO_IOMMU_MAP_F_READ |
                       VIRTIO_IOMMU_MAP_F_WRITE;
    req->tail.status = 0xFF;

    uint64_t req_phys = vv_virt_to_phys(req);
    size_t   in_len   = (size_t)((uint8_t *)&req->tail - (uint8_t *)req);
    uint64_t tail_phys = req_phys + in_len;

    vring_raw_set_desc(vr, 0, req_phys, in_len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, tail_phys, sizeof(req->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0016, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_map_phys_wrap,
              "Map with phys_start that wraps 64 bits",
              VIRTIO_SPEC_V1_2, "5.13.6.3");
