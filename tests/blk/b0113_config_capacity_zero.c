/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0113: blk_config_capacity_read
 *
 * Read capacity from device config space, then submit a read to
 * sector 0 when capacity is reported as nonzero. Baseline correctness
 * test for config space access and normal read path.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_config_capacity(struct virtio_dev *dev,
                                              struct vring *vr)
{
    /* Read capacity from device-specific config */
    volatile struct virtio_blk_config *blkcfg =
        (volatile struct virtio_blk_config *)dev->device_cfg;

    __sync_synchronize();
    uint64_t capacity = blkcfg->capacity;

    if (capacity == 0)
        return TEST_SKIP;

    /* Submit a basic read to sector 0 */
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0113, VIRTIO_PCI_DEVICE_BLK, test_blk_config_capacity,
              "Read config capacity then read sector 0",
              VIRTIO_SPEC_V1_2, "5.2.5.1");
