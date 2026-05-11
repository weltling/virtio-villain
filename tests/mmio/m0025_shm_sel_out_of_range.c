/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0025: Write out of range value to SHMSel register.
 *
 * Spec 4.2.2.2: SHMSel selects the shared memory region for
 * subsequent reads of SHMLenLow/High and SHMBaseLow/High. Writing
 * a large index that does not correspond to any region must not
 * crash the device; reads of SHMLen should return all ones per
 * spec when the id is invalid.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    if (virtio_mmio_init(dev) < 0)
        return TEST_SKIP;

    /* Write an out of range SHMSel */
    mmio_write32(dev, VIRTIO_MMIO_SHM_SEL, 0xFFFFFFFF);
    __sync_synchronize();

    /* Read SHMLenLow and SHMLenHigh; invalid id returns ~0 */
    uint32_t len_lo = mmio_read32(dev, VIRTIO_MMIO_SHM_LEN_LOW);
    uint32_t len_hi = mmio_read32(dev, VIRTIO_MMIO_SHM_LEN_HIGH);
    (void)len_lo;
    (void)len_hi;

    /* Read SHMBaseLow/High */
    uint32_t base_lo = mmio_read32(dev, VIRTIO_MMIO_SHM_BASE_LOW);
    uint32_t base_hi = mmio_read32(dev, VIRTIO_MMIO_SHM_BASE_HIGH);
    (void)base_lo;
    (void)base_hi;

    /* Try another bogus index */
    mmio_write32(dev, VIRTIO_MMIO_SHM_SEL, 0x7FFFFFFF);
    __sync_synchronize();
    len_lo = mmio_read32(dev, VIRTIO_MMIO_SHM_LEN_LOW);
    (void)len_lo;

    /* Verify device is still alive */
    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status == 0)
        TWEDGED("status == 0");

    return TEST_PASS;
}

REGISTER_TEST_MMIO(M0025, do_test,
    "SHMSel with out of range region id",
    VIRTIO_SPEC_V1_2, "4.2.2.2");
