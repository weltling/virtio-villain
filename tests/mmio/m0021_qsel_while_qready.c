/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0021: write QueueSel while QueueReady=1 (spec 4.2.3.2)
 *
 * Spec 4.2.3.2 requires the driver to clear QueueReady before
 * selecting another queue. Issue a QueueSel write while the
 * previously selected queue still reports QueueReady=1. The
 * device must keep the live queue intact and must not switch
 * the configuration of the previously enabled queue.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    if (virtio_mmio_init(dev) < 0)
        return TEST_SKIP;

    /* Configure queue 0 */
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
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_READY, 1);
    __sync_synchronize();

    /* Switch to queue 1 without clearing QueueReady on queue 0 */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_SEL, 1);
    __sync_synchronize();

    /* Read QueueReady on the new selection; this should be 0 */
    uint32_t ready1 = mmio_read32(dev, VIRTIO_MMIO_QUEUE_READY);
    if (ready1 != 0)
        TFAIL("ready1 != 0");

    /* Re select queue 0 and confirm it still reports ready */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_SEL, 0);
    __sync_synchronize();
    uint32_t ready0 = mmio_read32(dev, VIRTIO_MMIO_QUEUE_READY);
    if (ready0 != 1)
        TFAIL("ready0 != 1");

    if (mmio_read32(dev, VIRTIO_MMIO_STATUS) == 0)
        TWEDGED("mmio_read32(dev, VIRTIO_MMIO_STATUS) == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST_MMIO(M0021, do_test,
    "QueueSel write while previous queue is still ready",
    VIRTIO_SPEC_V1_2, "4.2.3.2");
