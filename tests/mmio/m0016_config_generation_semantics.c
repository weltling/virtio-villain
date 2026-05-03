/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0016: ConfigGeneration counter semantics (spec 4.2.2.2)
 *
 * The ConfigGeneration register provides atomicity for config reads.
 * Verify it reads consistently and doesn't spuriously change.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    if (virtio_mmio_init(dev) < 0)
        return TEST_SKIP;

    /* Read config generation multiple times - should be stable */
    uint32_t gen1 = mmio_read32(dev, VIRTIO_MMIO_CONFIG_GENERATION);
    uint32_t gen2 = mmio_read32(dev, VIRTIO_MMIO_CONFIG_GENERATION);
    uint32_t gen3 = mmio_read32(dev, VIRTIO_MMIO_CONFIG_GENERATION);

    /* Generation should be stable when no config changes occur */
    if (gen1 != gen2 || gen2 != gen3)
        TFAIL("gen1 != gen2 || gen2 != gen3");

    /* Read some config space fields at offset 0x100+ */
    volatile uint32_t cfg0 = mmio_read32(dev, VIRTIO_MMIO_CONFIG);
    (void)cfg0;

    /* Check generation didn't change from just reading */
    uint32_t gen4 = mmio_read32(dev, VIRTIO_MMIO_CONFIG_GENERATION);
    if (gen4 != gen1)
        TFAIL("gen4 != gen1");

    /* Try to write to ConfigGeneration (read-only, must not crash) */
    mmio_write32(dev, VIRTIO_MMIO_CONFIG_GENERATION, 0xFF);
    __sync_synchronize();

    uint32_t gen5 = mmio_read32(dev, VIRTIO_MMIO_CONFIG_GENERATION);
    /* Should still be same value (write was ignored) */
    if (gen5 != gen1)
        TFAIL("gen5 != gen1");

    /* Verify device still alive */
    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status == 0)
        TWEDGED("status == 0");

    return TEST_PASS;
}

REGISTER_TEST_MMIO(M0016, do_test,
    "ConfigGeneration stability and read-only enforcement",
    VIRTIO_SPEC_V1_2, "4.2.2.2");
