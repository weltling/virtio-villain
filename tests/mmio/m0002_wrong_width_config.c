/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0002: Wrong width access to config space fields (spec 4.2.2.2)
 *
 * Device-specific configuration (offset 0x100+) may have fields of
 * varying widths, but the MMIO transport mandates 32-bit register
 * access. This test performs 8-bit and 16-bit accesses to the
 * device configuration space area. The VMM must not crash.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    if (virtio_mmio_init(dev) < 0)
        return TEST_SKIP;

    /*
     * Access device-specific config at offset 0x100.
     * For a block device this is capacity (8 bytes).
     * Wrong-width access should not crash the VMM.
     */
    volatile uint8_t val8;
    volatile uint16_t val16;

    /* 8-bit reads into config space */
    val8 = mmio_read8(dev, VIRTIO_MMIO_CONFIG + 0);
    (void)val8;
    val8 = mmio_read8(dev, VIRTIO_MMIO_CONFIG + 1);
    (void)val8;
    val8 = mmio_read8(dev, VIRTIO_MMIO_CONFIG + 7);
    (void)val8;

    /* 16-bit reads into config space */
    val16 = mmio_read16(dev, VIRTIO_MMIO_CONFIG + 0);
    (void)val16;
    val16 = mmio_read16(dev, VIRTIO_MMIO_CONFIG + 2);
    (void)val16;

    /* 8-bit write to config space (normally read-only for block) */
    mmio_write8(dev, VIRTIO_MMIO_CONFIG + 0, 0xFF);

    /* 16-bit write to config space */
    mmio_write16(dev, VIRTIO_MMIO_CONFIG + 0, 0xFFFF);

    /* Unaligned 16-bit access (odd offset) */
    val16 = mmio_read16(dev, VIRTIO_MMIO_CONFIG + 1);
    (void)val16;

    /* Verify device is still alive */
    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status == 0)
        TWEDGED("status == 0");

    TREJECT("no device response within timeout");
}

REGISTER_TEST_MMIO(M0002, do_test,
    "Wrong width access to MMIO config space fields",
    VIRTIO_SPEC_V1_2, "4.2.2.2");
