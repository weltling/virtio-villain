/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0064: MAP with virt_start == virt_end (single byte).
 *
 * The spec inclusive range allows virt_start == virt_end,
 * describing a 1-byte mapping. Most implementations require
 * page-aligned ranges and so reject, but the device must not
 * crash. Different from I0024 which uses a non-zero offset.
 *
 * Spec 5.13.5.6.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_map_one_byte(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_iommu_req_map *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->head.type   = VIRTIO_IOMMU_T_MAP;
    req->domain      = 0;
    req->virt_start  = 0x100000;
    req->virt_end    = 0x100000;
    req->phys_start  = 0x80000;
    req->flags       = VIRTIO_IOMMU_MAP_F_READ;
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

REGISTER_TEST(I0064, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_map_one_byte,
              "Map with virt_start == virt_end",
              VIRTIO_SPEC_V1_2, "5.13.5.6");
