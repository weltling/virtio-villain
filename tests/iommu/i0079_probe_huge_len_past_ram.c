/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0079: IOMMU probe writable response len past end of guest RAM.
 *
 * Same shape as RNG0004: submit a PROBE whose writable output
 * descriptor base lives in valid guest RAM but whose length
 * crosses the end of all System RAM. Device must not access
 * memory outside the guest's mapping or crash the VMM.
 *
 * Spec 5.13.6.5.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_probe_huge_len_past_ram(struct virtio_dev *dev,
                                                        struct vring *vr)
{
    uint64_t ram_top = vv_parse_ram_top();
    if (ram_top == 0)
        return TEST_SKIP;

    struct virtio_iommu_req_probe *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->head.type   = VIRTIO_IOMMU_T_PROBE;
    req->endpoint    = 0;
    req->tail.status = 0xFF;

    uint64_t req_phys = vv_virt_to_phys(req);
    size_t   in_len   = (size_t)((uint8_t *)&req->properties - (uint8_t *)req);
    uint64_t out_phys;
    if (!vv_alloc_page_near_ram_top(ram_top, &out_phys))
        return TEST_SKIP;

    uint64_t overshoot = (ram_top - out_phys) + (1ULL << 30);
    if (overshoot > 0xFFFFFFFFULL)
        overshoot = 0xFFFFFFFFULL;
    uint32_t len = (uint32_t)overshoot;

    vring_raw_set_desc(vr, 0, req_phys, in_len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, out_phys, len,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0079, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_probe_huge_len_past_ram,
              "IOMMU probe writable response len crosses end of RAM",
              VIRTIO_SPEC_V1_2, "5.13.6.5");
