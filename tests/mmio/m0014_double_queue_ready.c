/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0014: Double QueueReady on same queue (spec 4.2.3.2)
 *
 * After a queue is enabled (QueueReady=1), writing QueueReady=1
 * again is a driver error. The VMM must handle this gracefully.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    if (virtio_mmio_init(dev) < 0)
        return TEST_SKIP;

    /* Select queue 0 */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_SEL, 0);
    __sync_synchronize();

    uint32_t max = mmio_read32(dev, VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max == 0)
        return TEST_SKIP;

    /* Configure queue properly */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_NUM, 16);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DESC_LOW, 0x10000);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DESC_HIGH, 0);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DRIVER_LOW, 0x20000);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DRIVER_HIGH, 0);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DEVICE_LOW, 0x30000);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DEVICE_HIGH, 0);
    __sync_synchronize();

    /* First QueueReady */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_READY, 1);
    __sync_synchronize();

    /* Second QueueReady (violation - already enabled) */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_READY, 1);
    __sync_synchronize();

    /* Third time with different value */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_READY, 0xFFFFFFFF);
    __sync_synchronize();

    /* Verify device still alive */
    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status == 0)
        TWEDGED("status == 0");

    TREJECT("no device response within timeout");
}

REGISTER_TEST_MMIO(M0014, do_test,
    "Double QueueReady on already-enabled queue",
    VIRTIO_SPEC_V1_2, "4.2.3.2");
