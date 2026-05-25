/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0085: MAP with phys_start unaligned to page boundary.
 *
 * Spec 5.13.6.3: virt_start and phys_start identify the start
 * of the virtual and physical ranges to map. Submit a MAP with
 * a page aligned virt_start but a phys_start that carries
 * arbitrary low bits set. The device must reject the unaligned
 * physical address cleanly rather than truncating the low bits
 * and silently mapping a different physical page.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_map_unaligned_phys(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    /* Attach endpoint 0 to a fresh domain first. */
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
    req->virt_start  = 0x10000000ULL;
    req->virt_end    = 0x10000FFFULL;
    req->phys_start  = 0x100123ULL;  /* low 12 bits set */
    req->flags       = VIRTIO_IOMMU_MAP_F_READ |
                       VIRTIO_IOMMU_MAP_F_WRITE;
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

REGISTER_TEST(I0085, VIRTIO_PCI_DEVICE_IOMMU,
              test_iommu_map_unaligned_phys,
              "MAP with phys_start unaligned to page",
              VIRTIO_SPEC_V1_2, "5.13.6.3");
