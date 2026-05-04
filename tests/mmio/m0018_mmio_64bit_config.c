/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0018: Read a 64-bit config value as two separate 32-bit reads
 * without checking ConfigGeneration between them.
 *
 * Spec 4.2.2.2: The driver should check config_generation to ensure
 * a consistent view of config space. This test intentionally skips
 * that check to exercise device behavior when config may be torn.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    /* Initialize device minimally */
    mmio_write32(dev, VIRTIO_MMIO_STATUS, 0);
    __sync_synchronize();
    usleep(10000);

    mmio_write32(dev, VIRTIO_MMIO_STATUS, VIRTIO_MMIO_STATUS_ACKNOWLEDGE);
    __sync_synchronize();
    mmio_write32(dev, VIRTIO_MMIO_STATUS,
                 VIRTIO_MMIO_STATUS_ACKNOWLEDGE | VIRTIO_MMIO_STATUS_DRIVER);
    __sync_synchronize();

    /* Accept no features */
    mmio_write32(dev, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
    mmio_write32(dev, VIRTIO_MMIO_DRIVER_FEATURES, 0);
    mmio_write32(dev, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 1);
    mmio_write32(dev, VIRTIO_MMIO_DRIVER_FEATURES, 0);
    __sync_synchronize();

    mmio_write32(dev, VIRTIO_MMIO_STATUS,
                 VIRTIO_MMIO_STATUS_ACKNOWLEDGE |
                 VIRTIO_MMIO_STATUS_DRIVER |
                 VIRTIO_MMIO_STATUS_FEATURES_OK);
    __sync_synchronize();
    usleep(5000);

    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_MMIO_STATUS_FEATURES_OK))
        return TEST_SKIP;

    /*
     * Read the first 64-bit config value as two 32-bit reads.
     * Config space starts at offset 0x100. For a block device,
     * the first field is "capacity" (64-bit, little-endian).
     * We intentionally do NOT check ConfigGeneration between reads.
     */
    uint32_t lo = mmio_read32(dev, VIRTIO_MMIO_CONFIG + 0);
    uint32_t hi = mmio_read32(dev, VIRTIO_MMIO_CONFIG + 4);

    /* Combine into 64-bit value */
    uint64_t capacity = ((uint64_t)hi << 32) | lo;

    /* Sanity: capacity should be non-zero for a real device */
    if (capacity == 0)
        return TEST_SKIP;

    /* Read again to verify stability */
    uint32_t lo2 = mmio_read32(dev, VIRTIO_MMIO_CONFIG + 0);
    uint32_t hi2 = mmio_read32(dev, VIRTIO_MMIO_CONFIG + 4);
    uint64_t capacity2 = ((uint64_t)hi2 << 32) | lo2;

    /* Both reads should be consistent in a quiescent state */
    if (capacity != capacity2)
        TFAIL("capacity != capacity2");

    return TEST_PASS;
}

REGISTER_TEST_MMIO(M0018, do_test,
    "64-bit config read as two 32-bit reads without ConfigGeneration check",
    VIRTIO_SPEC_V1_2, "4.2.2.2");
