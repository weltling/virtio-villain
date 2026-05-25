/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0029: MMIO QueueReady written with a non boolean value.
 *
 * Spec 4.2.2: QueueReady is defined to take values 0 (disabled)
 * or 1 (enabled). Write 0xFF and read the value back. The device
 * must either mask to the low bit or reject the write and must
 * stay alive afterwards.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    if (virtio_mmio_init(dev) < 0)
        return TEST_SKIP;

    mmio_write32(dev, VIRTIO_MMIO_QUEUE_SEL, 0);
    __sync_synchronize();
    uint32_t max = mmio_read32(dev, VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max == 0)
        return TEST_SKIP;

    mmio_write32(dev, VIRTIO_MMIO_QUEUE_NUM, 16);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DESC_LOW, 0x10000);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DESC_HIGH, 0);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DRIVER_LOW, 0x20000);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DRIVER_HIGH, 0);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DEVICE_LOW, 0x30000);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DEVICE_HIGH, 0);
    __sync_synchronize();

    /* Write 0xFF instead of the spec defined 0 or 1. */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_READY, 0xFFu);
    __sync_synchronize();

    uint32_t v = mmio_read32(dev, VIRTIO_MMIO_QUEUE_READY);
    if (v == 0xFFu)
        TFAIL("QueueReady retained 0xFF verbatim");

    if (mmio_read32(dev, VIRTIO_MMIO_STATUS) == 0)
        TWEDGED("device wedged after non boolean QueueReady write");

    return TEST_PASS;
}

REGISTER_TEST_MMIO(M0029, do_test,
    "MMIO QueueReady written with non boolean value",
    VIRTIO_SPEC_V1_2, "4.2.2");
