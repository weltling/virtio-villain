/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0006: Skip MagicValue/Version check at init (spec 4.2.3.1.1)
 *
 * The spec states the driver MUST check MagicValue returns 0x74726976
 * and Version returns 2 before proceeding. This test skips that
 * validation and proceeds directly to device initialization,
 * operating on the device as if MagicValue and Version were correct.
 * This exercises the VMM's tolerance of a driver that skips
 * required checks. The VMM must not crash.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    /*
     * Deliberately skip reading/checking MagicValue and Version.
     * Go straight to setting status bits.
     */

    /* Reset */
    mmio_write32(dev, VIRTIO_MMIO_STATUS, 0);
    __sync_synchronize();

    /* ACKNOWLEDGE without checking magic/version */
    mmio_write32(dev, VIRTIO_MMIO_STATUS, VIRTIO_MMIO_STATUS_ACKNOWLEDGE);
    __sync_synchronize();

    /* DRIVER */
    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    mmio_write32(dev, VIRTIO_MMIO_STATUS,
                 status | VIRTIO_MMIO_STATUS_DRIVER);
    __sync_synchronize();

    /* Skip feature negotiation entirely, go straight to FEATURES_OK */
    status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    mmio_write32(dev, VIRTIO_MMIO_STATUS,
                 status | VIRTIO_MMIO_STATUS_FEATURES_OK);
    __sync_synchronize();

    /* Skip checking if FEATURES_OK was accepted, set DRIVER_OK */
    status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    mmio_write32(dev, VIRTIO_MMIO_STATUS,
                 status | VIRTIO_MMIO_STATUS_DRIVER_OK);
    __sync_synchronize();

    /* Try to kick without any queue setup */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_NOTIFY, 0);

    /* Verify device hasn't crashed */
    status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status == 0)
        TWEDGED("status == 0");

    TREJECT("no device response within timeout");
}

REGISTER_TEST_MMIO(M0006, do_test,
    "Skip MagicValue/Version check at MMIO init",
    VIRTIO_SPEC_V1_2, "4.2.3.1.1");
