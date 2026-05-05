/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0007: IOMMU request with unknown type byte.
 *
 * Set head.type to a value outside the defined T_ATTACH..T_PROBE
 * range. The device must respond with UNSUPP and stay alive.
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

static test_result_t test_iommu_unknown_type(struct virtio_dev *dev,
                                             struct vring *vr)
{
    /* A buffer large enough for any defined request, plus the tail. */
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 256);

    struct virtio_iommu_req_head *head = (struct virtio_iommu_req_head *)buf;
    head->type = 0xAA;
    struct virtio_iommu_req_tail *tail =
        (struct virtio_iommu_req_tail *)(buf + 60);
    tail->status = 0xFF;

    uint64_t buf_phys  = vv_virt_to_phys(buf);
    uint64_t tail_phys = buf_phys + 60;

    vring_raw_set_desc(vr, 0, buf_phys, 60,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, tail_phys, sizeof(*tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(I0007, VIRTIO_PCI_DEVICE_IOMMU, test_iommu_unknown_type,
              "Request with unknown type byte",
              VIRTIO_SPEC_V1_2, "5.13.6");
