/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0028: Write to SHM read only registers.
 *
 * Spec 4.2.2.2: SHMLenLow, SHMLenHigh, SHMBaseLow, and SHMBaseHigh
 * are read only registers. Writing garbage to them must not crash
 * the device and the values must remain unchanged.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    if (virtio_mmio_init(dev) < 0)
        return TEST_SKIP;

    mmio_write32(dev, VIRTIO_MMIO_SHM_SEL, 0);
    __sync_synchronize();

    uint32_t len_lo = mmio_read32(dev, VIRTIO_MMIO_SHM_LEN_LOW);
    uint32_t base_lo = mmio_read32(dev, VIRTIO_MMIO_SHM_BASE_LOW);

    /* Write garbage to read only SHM registers */
    mmio_write32(dev, VIRTIO_MMIO_SHM_LEN_LOW, 0xDEADBEEF);
    mmio_write32(dev, VIRTIO_MMIO_SHM_LEN_HIGH, 0xDEADBEEF);
    mmio_write32(dev, VIRTIO_MMIO_SHM_BASE_LOW, 0xCAFEBABE);
    mmio_write32(dev, VIRTIO_MMIO_SHM_BASE_HIGH, 0xCAFEBABE);
    __sync_synchronize();

    /* Values must be unchanged */
    if (mmio_read32(dev, VIRTIO_MMIO_SHM_LEN_LOW) != len_lo)
        TFAIL("mmio_read32(dev, VIRTIO_MMIO_SHM_LEN_LOW) != len_lo");
    if (mmio_read32(dev, VIRTIO_MMIO_SHM_BASE_LOW) != base_lo)
        TFAIL("mmio_read32(dev, VIRTIO_MMIO_SHM_BASE_LOW) != base_lo");

    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status == 0)
        TWEDGED("status == 0");

    TREJECT("no device response within timeout");
}

REGISTER_TEST_MMIO(M0028, do_test,
    "Write to read only SHM registers",
    VIRTIO_SPEC_V1_2, "4.2.2.2");
