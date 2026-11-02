/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0085: Read with sector+len exactly at capacity (boundary hit)
 *
 * Spec 5.2.6.1: request must not go beyond capacity. Submit a
 * read where sector + (data_len/512) == capacity exactly. This
 * should succeed (it's the last valid range), testing that the
 * device correctly handles the boundary without off-by-one.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_read_last_valid(struct virtio_dev *dev,
                                              struct vring *vr)
{
    /* Read capacity from device config */
    uint64_t capacity = 0;
    volatile uint8_t *cfg_space = dev->device_cfg;
    for (int i = 0; i < 8; i++)
        ((uint8_t *)&capacity)[i] = cfg_space[i];

    if (capacity < 1)
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->sector = capacity - 1; /* last valid sector */
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0085, VIRTIO_PCI_DEVICE_BLK, test_blk_read_last_valid,
              "Read last valid sector (sector = capacity - 1)",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
