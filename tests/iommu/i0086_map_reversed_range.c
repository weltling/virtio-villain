/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0086: MAP with virt_end < virt_start.
 *
 * Spec 5.13.6.3: virt_start and virt_end form a closed range
 * with virt_start <= virt_end. Submit a MAP whose virt_end is
 * less than virt_start so the device computing length as
 * virt_end - virt_start + 1 would underflow into a huge value.
 * The device must reject the reversed range rather than walking
 * an astronomically large span and exhausting host memory.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_map_reversed_range(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_iommu_req_attach *att = vv_alloc_pages(1);
    memset(att, 0, sizeof(*att));
    att->head.type   = VIRTIO_IOMMU_T_ATTACH;
    att->domain      = 1;
    att->endpoint    = 0;
    att->tail.status = 0xFF;

    uint64_t abase = vv_virt_to_phys(att);
    size_t a_in    = (size_t)((uint8_t *)&att->tail - (uint8_t *)att);

    vring_raw_set_desc(vr, 0, abase, (uint32_t)a_in,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, abase + a_in, sizeof(att->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    struct virtio_iommu_req_map *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    req->head.type   = VIRTIO_IOMMU_T_MAP;
    req->domain      = 1;
    req->virt_start  = 0xFFFFFFFF000ULL;
    req->virt_end    = 0x10000ULL;
    req->phys_start  = 0x100000ULL;
    req->flags       = VIRTIO_IOMMU_MAP_F_READ;
    req->tail.status = 0xFF;

    uint64_t base = vv_virt_to_phys(req);
    size_t in_len = (size_t)((uint8_t *)&req->tail - (uint8_t *)req);

    vring_raw_set_desc(vr, 2, base, (uint32_t)in_len,
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, base + in_len, sizeof(req->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0086, VIRTIO_PCI_DEVICE_IOMMU,
              test_iommu_map_reversed_range,
              "MAP with virt_end strictly less than virt_start",
              VIRTIO_SPEC_V1_2, "5.13.6.3");
