/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0025: IOMMU oversized request body.
 *
 * Submit an ATTACH whose readable descriptor is twice the
 * spec mandated size. The device must either consume only the
 * portion it understands or reject the request, never read
 * past the end of its own request structure.
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

static test_result_t test_iommu_oversized_input(struct virtio_dev *dev,
                                                struct vring *vr)
{
    /* Allocate a buffer wider than any defined request. */
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0xCD, 256);

    struct virtio_iommu_req_head *head = (struct virtio_iommu_req_head *)buf;
    head->type = VIRTIO_IOMMU_T_ATTACH;
    head->reserved[0] = 0;
    head->reserved[1] = 0;
    head->reserved[2] = 0;

    /* Place a tail at offset 200 to receive the status. */
    struct virtio_iommu_req_tail *tail =
        (struct virtio_iommu_req_tail *)(buf + 200);
    tail->status = 0xFF;

    uint64_t buf_phys  = vv_virt_to_phys(buf);
    uint64_t tail_phys = buf_phys + 200;

    vring_raw_set_desc(vr, 0, buf_phys, 200,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, tail_phys, sizeof(*tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0025, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_oversized_input,
              "Request with oversized input descriptor",
              VIRTIO_SPEC_V1_2, "5.13.6");
