/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0035: MMIO Status register readback after write.
 *
 * Spec 4.2.2.2: Write ACKNOWLEDGE to the Status register and read
 * it back. The value must reflect the written bits.
 */
#include "tests/test.h"
#include "lib/virtio_mmio.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    /* Write ACKNOWLEDGE (1) */
    mmio_write32(dev, VIRTIO_MMIO_STATUS, 1);
    uint32_t val = mmio_read32(dev, VIRTIO_MMIO_STATUS);

    if (!(val & 1))
        TFAIL("Status readback 0x%x after writing ACKNOWLEDGE", val);

    /* Write DRIVER (2) additionally */
    mmio_write32(dev, VIRTIO_MMIO_STATUS, val | 2);
    val = mmio_read32(dev, VIRTIO_MMIO_STATUS);

    if (!(val & 2))
        TFAIL("Status readback 0x%x missing DRIVER bit", val);

    return TEST_PASS;
}

REGISTER_TEST_MMIO(M0035, do_test,
    "MMIO Status register readback after write",
    VIRTIO_SPEC_V1_2, "4.2.2.2");
