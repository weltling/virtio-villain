/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0030: MMIO queue with QueueDesc set to 0xFFFFFFFFFFFFFFC0.
 *
 * Spec 4.2.2.2 and 4.2.3.2: QueueDescLow/High give the guest
 * physical address of the descriptor area. Program a value near
 * the top of the 64 bit address space, set QueueReady, and write
 * QueueNotify. A device that adds the descriptor index to this
 * base without overflow checks will wrap to a low address and
 * may fetch unrelated memory or crash. The device must reject
 * the impossible base or stay safely idle.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    if (virtio_mmio_init(dev) < 0)
        return TEST_SKIP;

    mmio_write32(dev, VIRTIO_MMIO_QUEUE_SEL, 0);
    __sync_synchronize();
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_NUM, 64);

    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DESC_LOW,   0xFFFFFFC0u);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DESC_HIGH,  0xFFFFFFFFu);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DRIVER_LOW, 0xFFFFF000u);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DRIVER_HIGH,0xFFFFFFFFu);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DEVICE_LOW, 0xFFFFE000u);
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_DEVICE_HIGH,0xFFFFFFFFu);
    __sync_synchronize();

    mmio_write32(dev, VIRTIO_MMIO_QUEUE_READY, 1);
    __sync_synchronize();

    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    mmio_write32(dev, VIRTIO_MMIO_STATUS,
                 status | VIRTIO_MMIO_STATUS_DRIVER_OK);
    __sync_synchronize();

    mmio_write32(dev, VIRTIO_MMIO_QUEUE_NOTIFY, 0);
    __sync_synchronize();

    status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status == 0)
        TWEDGED("status == 0");

    TREJECT("no device response within timeout");
}

REGISTER_TEST_MMIO(M0030, do_test,
    "QueueDesc at top of 64 bit address space",
    VIRTIO_SPEC_V1_2, "4.2.3.2");
