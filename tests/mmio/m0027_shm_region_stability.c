/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0027: Read SHM region 0 and verify length and base stability.
 *
 * Spec 4.2.2.2: Select SHM region 0 and read its length and base
 * registers multiple times. The values must remain consistent
 * across reads when no config change is occurring.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    if (virtio_mmio_init(dev) < 0)
        return TEST_SKIP;

    mmio_write32(dev, VIRTIO_MMIO_SHM_SEL, 0);
    __sync_synchronize();

    uint32_t len_lo1 = mmio_read32(dev, VIRTIO_MMIO_SHM_LEN_LOW);
    uint32_t len_hi1 = mmio_read32(dev, VIRTIO_MMIO_SHM_LEN_HIGH);
    uint32_t base_lo1 = mmio_read32(dev, VIRTIO_MMIO_SHM_BASE_LOW);
    uint32_t base_hi1 = mmio_read32(dev, VIRTIO_MMIO_SHM_BASE_HIGH);

    /* Read again */
    mmio_write32(dev, VIRTIO_MMIO_SHM_SEL, 0);
    __sync_synchronize();

    uint32_t len_lo2 = mmio_read32(dev, VIRTIO_MMIO_SHM_LEN_LOW);
    uint32_t len_hi2 = mmio_read32(dev, VIRTIO_MMIO_SHM_LEN_HIGH);
    uint32_t base_lo2 = mmio_read32(dev, VIRTIO_MMIO_SHM_BASE_LOW);
    uint32_t base_hi2 = mmio_read32(dev, VIRTIO_MMIO_SHM_BASE_HIGH);

    if (len_lo1 != len_lo2 || len_hi1 != len_hi2)
        TFAIL("len_lo1 != len_lo2 || len_hi1 != len_hi2");
    if (base_lo1 != base_lo2 || base_hi1 != base_hi2)
        TFAIL("base_lo1 != base_lo2 || base_hi1 != base_hi2");

    /* Verify device alive */
    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status == 0)
        TWEDGED("status == 0");

    return TEST_PASS;
}

REGISTER_TEST_MMIO(M0027, do_test,
    "SHM region 0 length and base read stability",
    VIRTIO_SPEC_V1_2, "4.2.2.2");
