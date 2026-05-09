/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0076: iommu_map_below_input_range_start
 *
 * Submit MAP whose virt_start is below the device reported
 * input_range start. Spec 5.13.6.3 says ranges outside the
 * advertised input_range must be rejected. The device must
 * return RANGE without panicking.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_map_below_range(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_iommu_req_map *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->head.type = VIRTIO_IOMMU_T_MAP;
    req->domain = 1;
    req->virt_start = 0;
    req->virt_end = 0x1000 - 1;
    req->phys_start = 0;
    req->flags = VIRTIO_IOMMU_MAP_F_READ;
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

REGISTER_TEST(I0076, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_map_below_range,
              "MAP virt_start below device input_range start",
              VIRTIO_SPEC_V1_2, "5.13.6.3");
