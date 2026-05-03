/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0012: QueueNum exceeds QueueNumMax (spec 4.2.3.2)
 *
 * The spec says the driver MUST NOT write a value larger than
 * QueueNumMax to QueueNum. Test that the VMM handles it gracefully
 * when the driver violates this.
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
        return TEST_SKIP; /* no queues available */

    /* Write QueueNum = QueueNumMax + 1 (violation) */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_NUM, max + 1);
    __sync_synchronize();

    /* Write QueueNum = 0xFFFFFFFF (extreme) */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_NUM, 0xFFFFFFFF);
    __sync_synchronize();

    /* Write QueueNum = 0 */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_NUM, 0);
    __sync_synchronize();

    /* Try to make queue ready with bad size */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_NUM, max * 2);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DESC_LOW, 0x1000);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DESC_HIGH, 0);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DRIVER_LOW, 0x2000);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DRIVER_HIGH, 0);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DEVICE_LOW, 0x3000);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DEVICE_HIGH, 0);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_READY, 1);
    __sync_synchronize();

    /* Verify device still alive */
    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status == 0)
        TWEDGED("status == 0");

    TREJECT("no device response within timeout");
}

REGISTER_TEST_MMIO(M0012, do_test,
    "QueueNum set larger than QueueNumMax",
    VIRTIO_SPEC_V1_2, "4.2.3.2");
