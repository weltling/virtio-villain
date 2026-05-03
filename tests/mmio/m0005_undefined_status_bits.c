/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0005: Set undefined bits in status register (spec 4.2.2.2)
 *
 * The Status register has defined bits: ACKNOWLEDGE(1), DRIVER(2),
 * DRIVER_OK(4), FEATURES_OK(8), DEVICE_NEEDS_RESET(64), FAILED(128).
 * Bits 4-5 (0x10, 0x20) and bits above 7 (in a 32-bit register) are
 * reserved. Writing undefined bits must not crash the VMM.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    /* Reset first */
    mmio_write32(dev, VIRTIO_MMIO_STATUS, 0);
    __sync_synchronize();

    /* Start normal init */
    mmio_write32(dev, VIRTIO_MMIO_STATUS, VIRTIO_MMIO_STATUS_ACKNOWLEDGE);
    __sync_synchronize();

    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    mmio_write32(dev, VIRTIO_MMIO_STATUS,
                 status | VIRTIO_MMIO_STATUS_DRIVER);
    __sync_synchronize();

    /* Now write reserved/undefined bits */
    status = mmio_read32(dev, VIRTIO_MMIO_STATUS);

    /* Set bit 4 (0x10) - undefined */
    mmio_write32(dev, VIRTIO_MMIO_STATUS, status | 0x10);
    __sync_synchronize();

    /* Set bit 5 (0x20) - undefined */
    status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    mmio_write32(dev, VIRTIO_MMIO_STATUS, status | 0x20);
    __sync_synchronize();

    /* Set all high bits (0xFFFFFF00) - way beyond defined range */
    status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    mmio_write32(dev, VIRTIO_MMIO_STATUS, status | 0xFFFFFF00);
    __sync_synchronize();

    /* Write 0xFFFFFFFF - all bits set */
    mmio_write32(dev, VIRTIO_MMIO_STATUS, 0xFFFFFFFF);
    __sync_synchronize();

    /* Read back and check if device is still responsive */
    status = mmio_read32(dev, VIRTIO_MMIO_STATUS);

    /* Reset and verify recovery */
    mmio_write32(dev, VIRTIO_MMIO_STATUS, 0);
    __sync_synchronize();
    status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status != 0)
        TWEDGED("status != 0");

    /* Re-init to confirm device still works */
    mmio_write32(dev, VIRTIO_MMIO_STATUS, VIRTIO_MMIO_STATUS_ACKNOWLEDGE);
    __sync_synchronize();
    status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_MMIO_STATUS_ACKNOWLEDGE))
        TWEDGED("!(status & VIRTIO_MMIO_STATUS_ACKNOWLEDGE)");

    TREJECT("!(status & VIRTIO_MMIO_STATUS_ACKNOWLEDGE)");
}

REGISTER_TEST_MMIO(M0005, do_test,
    "Set undefined bits in MMIO status register",
    VIRTIO_SPEC_V1_2, "4.2.2.2");
