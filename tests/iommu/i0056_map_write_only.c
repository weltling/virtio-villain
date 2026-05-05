/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0056: MAP with WRITE flag only (no READ).
 *
 * Spec 5.13.6.3 lists READ and WRITE as independent
 * permission bits. A write-only mapping is unusual but valid.
 * The device must accept it.
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

static test_result_t test_iommu_map_write_only(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_iommu_req_map *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->head.type   = VIRTIO_IOMMU_T_MAP;
    req->domain      = 0;
    req->virt_start  = 0xD0000;
    req->virt_end    = 0xD0FFF;
    req->phys_start  = 0x80000;
    req->flags       = VIRTIO_IOMMU_MAP_F_WRITE;
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

REGISTER_TEST(I0056, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_map_write_only,
              "Map with WRITE flag only",
              VIRTIO_SPEC_V1_2, "5.13.6.3");
