/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0007: Missing InterruptACK after interrupt (spec 4.2.3.4.1)
 *
 * The spec states: "The driver MUST acknowledge all interrupts ...
 * by writing a corresponding bit to the InterruptACK register."
 * This test triggers an interrupt (via a queue notification that
 * the device may respond to) and deliberately does NOT write to
 * InterruptACK. The VMM must handle this gracefully without
 * wedging or crashing - the device may stop delivering further
 * interrupts but must remain functional.
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

    /*
     * Read InterruptStatus - may be 0 if no interrupt pending.
     * We'll attempt to provoke an interrupt by kicking.
     */
    uint32_t isr_before = mmio_read32(dev, VIRTIO_MMIO_INTERRUPT_STATUS);
    (void)isr_before;

    /* Kick queue 0 (even without proper queue setup) */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_NOTIFY, 0);

    /* Small delay for VMM to process */
    usleep(50000);

    /* Read InterruptStatus (might have changed) */
    uint32_t isr = mmio_read32(dev, VIRTIO_MMIO_INTERRUPT_STATUS);

    /*
     * Deliberately DO NOT write to InterruptACK.
     * This violates the spec. The VMM must cope.
     */
    (void)isr;

    /* Do more kicks without ACKing any interrupts */
    for (int i = 0; i < 10; i++) {
        mmio_write32(dev, VIRTIO_MMIO_QUEUE_NOTIFY, 0);
        usleep(10000);
    }

    /* Still don't ACK. Read interrupt status again. */
    isr = mmio_read32(dev, VIRTIO_MMIO_INTERRUPT_STATUS);
    (void)isr;

    /* Write a wrong value to InterruptACK (bits that weren't set) */
    mmio_write32(dev, VIRTIO_MMIO_INTERRUPT_ACK, 0xFFFFFFFF);

    /* Verify device is still alive */
    status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status == 0)
        TWEDGED("status == 0");

    TREJECT("no device response within timeout");
}

REGISTER_TEST_MMIO(M0007, do_test,
    "Missing InterruptACK after MMIO interrupt",
    VIRTIO_SPEC_V1_2, "4.2.3.4.1");
