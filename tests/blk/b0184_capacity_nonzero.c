/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0184: read capacity config is non zero.
 *
 * Spec 5.2.4: The capacity field is always present and expresses
 * the device size in 512 byte sectors. It must be non zero for
 * a functional block device.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_capacity_nonzero(struct virtio_dev *dev)
{
    if (!dev->device_cfg || dev->device_cfg_length < 8)
        return TEST_SKIP;

    volatile struct virtio_blk_config_head *bcfg =
        (volatile struct virtio_blk_config_head *)dev->device_cfg;

    uint64_t cap = bcfg->capacity;

    if (cap == 0)
        TFAIL("capacity is 0");

    return TEST_PASS;
}

REGISTER_TEST_DEV(B0184, VIRTIO_PCI_DEVICE_BLK, test_blk_capacity_nonzero,
                  "Capacity config field is non zero",
                  VIRTIO_SPEC_V1_2, "5.2.4");
