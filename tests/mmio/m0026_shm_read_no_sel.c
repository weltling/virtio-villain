/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0026: Read SHMLen and SHMBase without prior SHMSel write.
 *
 * Spec 4.2.2.2: The driver should write SHMSel before reading the
 * shared memory length and base registers. Reading without a prior
 * SHMSel write exercises the device default (typically id 0 or an
 * indeterminate selection). Must not crash.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    if (virtio_mmio_init(dev) < 0)
        return TEST_SKIP;

    /* Read SHM registers without writing SHMSel first */
    uint32_t len_lo = mmio_read32(dev, VIRTIO_MMIO_SHM_LEN_LOW);
    uint32_t len_hi = mmio_read32(dev, VIRTIO_MMIO_SHM_LEN_HIGH);
    uint32_t base_lo = mmio_read32(dev, VIRTIO_MMIO_SHM_BASE_LOW);
    uint32_t base_hi = mmio_read32(dev, VIRTIO_MMIO_SHM_BASE_HIGH);
    (void)len_lo;
    (void)len_hi;
    (void)base_lo;
    (void)base_hi;

    /* Verify device alive */
    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status == 0)
        TWEDGED("status == 0");

    return TEST_PASS;
}

REGISTER_TEST_MMIO(M0026, do_test,
    "Read SHM registers without prior SHMSel",
    VIRTIO_SPEC_V1_2, "4.2.2.2");
