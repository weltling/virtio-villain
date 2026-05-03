/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0004: Access queue regs without writing QueueSel first (spec 4.2.2.2)
 *
 * The spec states that the driver must write QueueSel before accessing
 * per-queue registers (QueueNumMax, QueueNum, QueueReady, QueueDesc*,
 * QueueDriver*, QueueDevice*). This test reads and writes queue registers
 * without first selecting a queue. The VMM must not crash.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    if (virtio_mmio_init(dev) < 0)
        return TEST_SKIP;

    /*
     * Do NOT write QueueSel. The queue selection register may
     * contain an indeterminate value or 0 from reset/init.
     * Access queue-specific registers directly.
     */

    volatile uint32_t val;

    /* Read QueueNumMax without selecting a queue */
    val = mmio_read32(dev, VIRTIO_MMIO_QUEUE_NUM_MAX);
    (void)val;

    /* Write QueueNum without selecting */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_NUM, 16);

    /* Write queue descriptor addresses without selecting */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DESC_LOW, 0x1000);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DESC_HIGH, 0);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DRIVER_LOW, 0x2000);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DRIVER_HIGH, 0);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DEVICE_LOW, 0x3000);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DEVICE_HIGH, 0);

    /* Try to enable a queue without QueueSel */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_READY, 1);

    /* Now select an out-of-range queue index and try same operations */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_SEL, 0xFFFF);

    val = mmio_read32(dev, VIRTIO_MMIO_QUEUE_NUM_MAX);
    (void)val;

    mmio_write32(dev, VIRTIO_MMIO_QUEUE_NUM, 256);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_READY, 1);

    /* Select another bogus queue index */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_SEL, 0x7FFFFFFF);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_READY, 1);

    /* Kick without selecting a valid queue */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_NOTIFY, 0xFFFF);

    /* Verify device is still alive */
    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status == 0)
        TWEDGED("status == 0");

    TREJECT("no device response within timeout");
}

REGISTER_TEST_MMIO(M0004, do_test,
    "Access queue regs without writing QueueSel first",
    VIRTIO_SPEC_V1_2, "4.2.2.2");
