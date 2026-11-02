/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0138: Write-zeroes spanning last sector (capacity boundary).
 *
 * Submit a WRITE_ZEROES request where sector + num_sectors reaches
 * exactly the device capacity boundary.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_write_zeroes_capacity_boundary(
    struct virtio_dev *dev, struct vring *vr)
{
    /* Read capacity from config space */
    volatile uint8_t *cfg_space = (volatile uint8_t *)dev->device_cfg;
    uint64_t capacity;
    memcpy(&capacity, (void *)cfg_space, sizeof(capacity));

    if (capacity < 2)
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_WRITE_ZEROES, 0);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* Request from last sector to end of device */
    seg->sector = capacity - 1;
    seg->num_sectors = 1;
    seg->flags = 0;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, seg, sizeof(*seg), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0138, VIRTIO_PCI_DEVICE_BLK, test_blk_write_zeroes_capacity_boundary,
              "Write-zeroes spanning last sector at capacity boundary",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
