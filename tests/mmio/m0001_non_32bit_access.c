/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0001: Non-32-bit access to control registers (spec 4.2.2.2)
 *
 * The spec states: "Each field ... can be read or written as a 32 bit
 * wide and aligned field." This test performs 8-bit and 16-bit reads
 * and writes to control registers. The VMM must not crash on
 * wrong-width accesses.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    /* First, do a valid init so the device is in a known state */
    if (virtio_mmio_init(dev) < 0)
        return TEST_SKIP;

    /*
     * Perform wrong-width reads. The device should either:
     * - Return 0 / garbage (implementation defined)
     * - Or honor the access
     * But it MUST NOT crash.
     */
    volatile uint8_t val8;
    volatile uint16_t val16;

    /* 8-bit read of MagicValue register (must be 32-bit access per spec) */
    val8 = mmio_read8(dev, VIRTIO_MMIO_MAGIC_VALUE);
    (void)val8;

    /* 16-bit read of Version register */
    val16 = mmio_read16(dev, VIRTIO_MMIO_VERSION);
    (void)val16;

    /* 8-bit read of Status register */
    val8 = mmio_read8(dev, VIRTIO_MMIO_STATUS);
    (void)val8;

    /* 16-bit read of DeviceID */
    val16 = mmio_read16(dev, VIRTIO_MMIO_DEVICE_ID);
    (void)val16;

    /*
     * Wrong-width writes.
     */
    /* 8-bit write to QueueSel (should be 32-bit) */
    mmio_write8(dev, VIRTIO_MMIO_QUEUE_SEL, 0);

    /* 16-bit write to QueueNum (should be 32-bit) */
    mmio_write16(dev, VIRTIO_MMIO_QUEUE_NUM, 8);

    /* 8-bit write to QueueNotify */
    mmio_write8(dev, VIRTIO_MMIO_QUEUE_NOTIFY, 0);

    /* Verify device is still alive by reading status */
    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status == 0)
        TWEDGED("status == 0");

    TREJECT("no device response within timeout");
}

REGISTER_TEST_MMIO(M0001, do_test,
    "Non-32-bit access to MMIO control registers",
    VIRTIO_SPEC_V1_2, "4.2.2.2");
