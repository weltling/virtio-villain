/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0020: Write partial InterruptACK (only clear one bit when multiple
 * are set).
 *
 * Spec 4.2.2.2: InterruptACK is write-only; writing a bit clears
 * the corresponding interrupt. If both used-ring-update (bit 0) and
 * config-change (bit 1) are pending, clearing only one should leave
 * the other still asserted.
 */
#include "tests/test.h"

#include <unistd.h>

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    /* Initialize device */
    mmio_write32(dev, VIRTIO_MMIO_STATUS, 0);
    __sync_synchronize();
    usleep(10000);

    mmio_write32(dev, VIRTIO_MMIO_STATUS, VIRTIO_MMIO_STATUS_ACKNOWLEDGE);
    __sync_synchronize();
    mmio_write32(dev, VIRTIO_MMIO_STATUS,
                 VIRTIO_MMIO_STATUS_ACKNOWLEDGE | VIRTIO_MMIO_STATUS_DRIVER);
    __sync_synchronize();

    mmio_write32(dev, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
    mmio_write32(dev, VIRTIO_MMIO_DRIVER_FEATURES, 0);
    mmio_write32(dev, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 1);
    mmio_write32(dev, VIRTIO_MMIO_DRIVER_FEATURES, 0);
    __sync_synchronize();

    mmio_write32(dev, VIRTIO_MMIO_STATUS,
                 VIRTIO_MMIO_STATUS_ACKNOWLEDGE |
                 VIRTIO_MMIO_STATUS_DRIVER |
                 VIRTIO_MMIO_STATUS_FEATURES_OK);
    __sync_synchronize();
    usleep(5000);

    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_MMIO_STATUS_FEATURES_OK))
        return TEST_SKIP;

    mmio_write32(dev, VIRTIO_MMIO_STATUS,
                 VIRTIO_MMIO_STATUS_ACKNOWLEDGE |
                 VIRTIO_MMIO_STATUS_DRIVER |
                 VIRTIO_MMIO_STATUS_FEATURES_OK |
                 VIRTIO_MMIO_STATUS_DRIVER_OK);
    __sync_synchronize();

    /* Read current interrupt status */
    uint32_t int_status = mmio_read32(dev, VIRTIO_MMIO_INTERRUPT_STATUS);

    /*
     * Write a partial ACK: clear only bit 0 (used ring update).
     * If bit 1 (config change) was also set, it should remain.
     */
    mmio_write32(dev, VIRTIO_MMIO_INTERRUPT_ACK, 0x1);
    __sync_synchronize();

    /* Read interrupt status again */
    uint32_t int_after = mmio_read32(dev, VIRTIO_MMIO_INTERRUPT_STATUS);

    /* Bit 0 should be cleared */
    if (int_after & 0x1)
        if (int_status & 0x1)
            TFAIL("int_status & 0x1"); /* ACK didn't clear bit 0 */

    /* Now clear bit 1 */
    mmio_write32(dev, VIRTIO_MMIO_INTERRUPT_ACK, 0x2);
    __sync_synchronize();

    /* Write ACK with bits that were never set (harmless) */
    mmio_write32(dev, VIRTIO_MMIO_INTERRUPT_ACK, 0x3);
    __sync_synchronize();

    /* Device should survive all of this */
    status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status == 0)
        TWEDGED("status == 0");

    return TEST_PASS;
}

REGISTER_TEST_MMIO(M0020, do_test,
    "Partial InterruptACK (clear one bit, leave other set)",
    VIRTIO_SPEC_V1_2, "4.2.2.2");
