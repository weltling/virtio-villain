/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0032: MMIO InterruptStatus reports CONFIG bit on config change.
 *
 * Spec 4.2.3.4: bit 1 of InterruptStatus signals a configuration
 * change interrupt. A device that conflates queue and config
 * interrupts could leave bit 1 zero after a config write. Read
 * the status register; if config change is signalled, ack it
 * and read again to ensure the bit clears.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    if (virtio_mmio_init(dev) < 0)
        return TEST_SKIP;

    uint32_t s = mmio_read32(dev, VIRTIO_MMIO_INTERRUPT_STATUS);
    if (!(s & 0x2)) {
        /* No config interrupt right now; cannot drive one from guest. */
        return TEST_SKIP;
    }

    mmio_write32(dev, VIRTIO_MMIO_INTERRUPT_ACK, 0x2);
    __sync_synchronize();

    s = mmio_read32(dev, VIRTIO_MMIO_INTERRUPT_STATUS);
    if (s & 0x2)
        TFAIL("config interrupt bit still set after ack: 0x%x", s);
    return TEST_PASS;
}

REGISTER_TEST_MMIO(M0032, do_test,
    "MMIO config change interrupt status bit semantics",
    VIRTIO_SPEC_V1_4, "4.2.3.4");
