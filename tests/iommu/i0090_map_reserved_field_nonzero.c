/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0090: MAP with reserved fields set to non zero.
 *
 * v1.4 5.13.6.3: reserved bytes in the MAP request must be
 * zero. Set them non zero and submit; the device must reject.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/virtio_iommu.h"

#include <string.h>

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_iommu_req_map *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    req->head.type = VIRTIO_IOMMU_T_MAP;
    req->head.reserved[0] = 0x55;
    req->head.reserved[1] = 0xAA;
    req->domain = 0;
    req->virt_start = 0;
    req->virt_end = 0xFFF;
    req->phys_start = 0;
    req->flags = VIRTIO_IOMMU_MAP_F_READ;
    req->tail.status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req),
                       (uint32_t)((uint8_t *)&req->tail - (uint8_t *)req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(&req->tail),
                       sizeof(req->tail), VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0090, VIRTIO_PCI_DEVICE_IOMMU, test,
              "MAP with reserved header bytes non zero",
              VIRTIO_SPEC_V1_4, "5.13.6.3");
