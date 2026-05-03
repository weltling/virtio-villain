/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0015: Queue reconfiguration after DRIVER_OK (spec 4.2.3.2)
 *
 * After DRIVER_OK is set, the driver should not reconfigure queues.
 * This test tries to change queue parameters after the device is live.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    if (virtio_mmio_init(dev) < 0)
        return TEST_SKIP;

    /* Set DRIVER_OK */
    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    mmio_write32(dev, VIRTIO_MMIO_STATUS,
                 status | VIRTIO_MMIO_STATUS_DRIVER_OK);
    __sync_synchronize();

    /* Select queue 0 and try to reconfigure */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_SEL, 0);
    __sync_synchronize();

    /* Change QueueNum after DRIVER_OK (violation) */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_NUM, 8);
    __sync_synchronize();

    /* Change queue addresses after DRIVER_OK (violation) */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DESC_LOW, 0x50000);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DESC_HIGH, 0);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DRIVER_LOW, 0x60000);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DRIVER_HIGH, 0);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DEVICE_LOW, 0x70000);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DEVICE_HIGH, 0);
    __sync_synchronize();

    /* Try setting QueueReady again */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_READY, 1);
    __sync_synchronize();

    /* Verify device still alive */
    status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status == 0)
        TWEDGED("status == 0");

    TREJECT("no device response within timeout");
}

REGISTER_TEST_MMIO(M0015, do_test,
    "Reconfigure queue after DRIVER_OK",
    VIRTIO_SPEC_V1_2, "4.2.3.2");
