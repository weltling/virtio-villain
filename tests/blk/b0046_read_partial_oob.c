/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0046: blk_read_partial_oob
 *
 * Issue a READ at sector == capacity-1 with a data buffer large enough
 * for 2 sectors (1024 bytes). The request starts in-bounds but extends
 * past the end. Tests device boundary checking for partial OOB reads.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_read_partial_oob(struct virtio_dev *dev,
                                               struct vring *vr)
{
    /* Read capacity from device config space */
    if (!dev->device_cfg)
        return TEST_SKIP;
    volatile uint64_t *cap = (volatile uint64_t *)dev->device_cfg;
    uint64_t capacity = *cap; /* in 512-byte sectors */

    if (capacity < 2)
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->sector = capacity - 1; /* last valid sector */
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    /* Request 1024 bytes = 2 sectors, but only 1 sector remains */
    vv_desc(vr, 1, data, 1024, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0046, VIRTIO_PCI_DEVICE_BLK, test_blk_read_partial_oob,
              "READ at last sector with 2-sector buffer (partial OOB)",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
