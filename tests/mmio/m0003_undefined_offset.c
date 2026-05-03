/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0003: Access MMIO register outside defined table (spec 4.2.2.2)
 *
 * The MMIO register map defines specific offsets. Accesses to
 * undefined/reserved offsets between or beyond defined registers
 * should return 0 or be ignored, but must not crash the VMM.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    if (virtio_mmio_init(dev) < 0)
        return TEST_SKIP;

    volatile uint32_t val;

    /*
     * Read from gaps in the register map.
     * Defined registers: 0x000-0x00c, 0x010-0x024, 0x030-0x038,
     * 0x044, 0x050, 0x060-0x064, 0x070, 0x080-0x084, 0x090-0x094,
     * 0x0a0-0x0a4, 0x0fc, 0x100+
     *
     * Gaps include: 0x028-0x02c, 0x03c-0x040, 0x048-0x04c,
     * 0x054-0x05c, 0x068-0x06c, 0x074-0x07c, etc.
     */

    /* Read undefined offset 0x028 */
    val = mmio_read32(dev, 0x028);
    (void)val;

    /* Read undefined offset 0x02c */
    val = mmio_read32(dev, 0x02c);
    (void)val;

    /* Read undefined offset 0x03c */
    val = mmio_read32(dev, 0x03c);
    (void)val;

    /* Read undefined offset 0x040 */
    val = mmio_read32(dev, 0x040);
    (void)val;

    /* Read undefined offset 0x048 */
    val = mmio_read32(dev, 0x048);
    (void)val;

    /* Read undefined offset 0x04c */
    val = mmio_read32(dev, 0x04c);
    (void)val;

    /* Read undefined offset 0x054 */
    val = mmio_read32(dev, 0x054);
    (void)val;

    /* Read undefined offset 0x074 */
    val = mmio_read32(dev, 0x074);
    (void)val;

    /* Write to undefined/reserved offsets */
    mmio_write32(dev, 0x028, 0xDEADBEEF);
    mmio_write32(dev, 0x03c, 0xCAFEBABE);
    mmio_write32(dev, 0x048, 0x12345678);
    mmio_write32(dev, 0x074, 0xFFFFFFFF);

    /* Read past the end of the standard register space (but before config) */
    val = mmio_read32(dev, 0x0c4);
    (void)val;
    val = mmio_read32(dev, 0x0d0);
    (void)val;
    val = mmio_read32(dev, 0x0f8);
    (void)val;

    /* Verify device is still alive */
    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status == 0)
        TWEDGED("status == 0");

    TREJECT("no device response within timeout");
}

REGISTER_TEST_MMIO(M0003, do_test,
    "Access MMIO register outside defined table",
    VIRTIO_SPEC_V1_2, "4.2.2.2");
