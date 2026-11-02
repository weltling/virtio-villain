/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0079: Write with sector exactly at capacity boundary
 *
 * Spec 5.2.6.1: "A driver MUST NOT submit a request which would
 * cause a read or write beyond capacity."
 *
 * Submit a 512-byte write at sector = capacity (one past the last
 * valid sector). The write touches sector [capacity, capacity+1)
 * which is beyond the device.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_write_at_capacity(struct virtio_dev *dev,
                                                struct vring *vr)
{
    /* Read capacity from device config (offset 0, 8 bytes) */
    uint64_t capacity = 0;
    volatile uint8_t *cfg_space = dev->device_cfg;
    for (int i = 0; i < 8; i++)
        ((uint8_t *)&capacity)[i] = cfg_space[i];

    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_OUT, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->sector = capacity; /* exactly at capacity = beyond last valid */
    memset(data, 0xDD, 512);
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0079, VIRTIO_PCI_DEVICE_BLK, test_blk_write_at_capacity,
              "Write at sector == capacity (one past last valid sector)",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
