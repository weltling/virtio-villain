/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0028: IOMMU rapid-fire request flood.
 *
 * Submit many ATTACH requests in a single batch (queue-size
 * worth) without waiting between them. Stresses the device
 * worker's batching, descriptor-table walk and used-ring
 * advancement.
 *
 * Spec 5.13.6.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_iommu_request_flood(struct virtio_dev *dev,
                                              struct vring *vr)
{
    /* Build 16 ATTACH requests back-to-back. */
    const int N = 16;
    uint8_t *buf = vv_alloc_pages(2);
    memset(buf, 0, 2 * 4096);

    size_t stride = sizeof(struct virtio_iommu_req_attach);

    for (int i = 0; i < N; i++) {
        struct virtio_iommu_req_attach *r =
            (struct virtio_iommu_req_attach *)(buf + i * stride);
        r->head.type = VIRTIO_IOMMU_T_ATTACH;
        r->domain    = 0;
        r->endpoint  = 0;
        r->tail.status = 0xFF;

        uint64_t r_phys = vv_virt_to_phys(buf) + i * stride;
        size_t   in_len = (size_t)((uint8_t *)&r->tail - (uint8_t *)r);

        vring_raw_set_desc(vr, i * 2, r_phys, in_len,
                           VRING_DESC_F_NEXT, i * 2 + 1);
        vring_raw_set_desc(vr, i * 2 + 1, r_phys + in_len, sizeof(r->tail),
                           VRING_DESC_F_WRITE, 0);

        vring_raw_set_avail(vr, i, i * 2);
    }
    vring_raw_set_avail_idx(vr, N);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0028, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_request_flood,
              "Flood the request queue with ATTACHes",
              VIRTIO_SPEC_V1_2, "5.13.6");
