/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0036: IOMMU MAP outside the device-advertised input range.
 *
 * If VIRTIO_IOMMU_F_INPUT_RANGE was offered, device config
 * exposes input_range.start/end. Submit a MAP whose virtual
 * range falls entirely above input_range.end (or below
 * input_range.start). Per spec 5.13.5.6 the device must reject
 * with VIRTIO_IOMMU_S_RANGE.
 *
 * Spec 5.13.4, 5.13.5.6.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_map_above_input_range(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    struct virtio_iommu_req_map *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->head.type   = VIRTIO_IOMMU_T_MAP;
    req->domain      = 0;
    /* Pick an absurdly high virtual range; if F_INPUT_RANGE is
     * negotiated this is necessarily above end. */
    req->virt_start  = 0xFFFFFFFFFFFF0000ULL;
    req->virt_end    = 0xFFFFFFFFFFFFFFFFULL;
    req->phys_start  = 0x80000;
    req->flags       = VIRTIO_IOMMU_MAP_F_READ;
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

REGISTER_TEST(I0036, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_map_above_input_range,
              "Map above device input range",
              VIRTIO_SPEC_V1_2, "5.13.5.6");
