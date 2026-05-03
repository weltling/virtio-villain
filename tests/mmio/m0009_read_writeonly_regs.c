/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0009: Read from write-only MMIO registers (spec 4.2.2.2)
 *
 * QueueNotify, InterruptACK, DriverFeatures, and DriverFeaturesSel
 * are write-only. Reading from them is undefined behavior per spec.
 * The VMM must not crash.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    if (virtio_mmio_init(dev) < 0)
        return TEST_SKIP;

    /* Read from write-only registers - result is undefined but must not crash */
    volatile uint32_t val;

    val = mmio_read32(dev, VIRTIO_MMIO_QUEUE_NOTIFY);
    (void)val;

    val = mmio_read32(dev, VIRTIO_MMIO_INTERRUPT_ACK);
    (void)val;

    val = mmio_read32(dev, VIRTIO_MMIO_DRIVER_FEATURES);
    (void)val;

    val = mmio_read32(dev, VIRTIO_MMIO_DRIVER_FEATURES_SEL);
    (void)val;

    /* Also read QueueSel (write-only) */
    val = mmio_read32(dev, VIRTIO_MMIO_QUEUE_SEL);
    (void)val;

    /* Read QueueNum (write-only per spec) */
    val = mmio_read32(dev, VIRTIO_MMIO_QUEUE_NUM);
    (void)val;

    /* Verify device is still alive */
    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status == 0)
        TWEDGED("status == 0");

    TREJECT("no device response within timeout");
}

REGISTER_TEST_MMIO(M0009, do_test,
    "Read from write-only MMIO registers",
    VIRTIO_SPEC_V1_2, "4.2.2.2");
