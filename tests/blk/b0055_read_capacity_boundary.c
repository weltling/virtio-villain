/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0055: read_at_capacity_boundary
 *
 * Read exactly the last valid sector (capacity - 1). This should
 * succeed since the request is entirely in-bounds.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_read_capacity_boundary(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    if (!dev->device_cfg)
        return TEST_SKIP;

    volatile uint64_t *cap = (volatile uint64_t *)dev->device_cfg;
    uint64_t capacity = *cap;

    if (capacity < 1)
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->sector = capacity - 1; /* exactly the last sector */
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0055, VIRTIO_PCI_DEVICE_BLK, test_blk_read_capacity_boundary,
              "Read exactly at last valid sector (capacity - 1)",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
