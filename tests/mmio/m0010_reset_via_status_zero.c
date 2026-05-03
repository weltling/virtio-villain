/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0010: Device reset via Status=0 (spec 4.2.3.1.1)
 *
 * Writing 0 to the Status register resets the device. After reset,
 * reading Status must return 0, DeviceID must still be valid, and
 * the device must be re-initializable.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    if (virtio_mmio_init(dev) < 0)
        return TEST_SKIP;

    /* Get device into DRIVER_OK state */
    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    mmio_write32(dev, VIRTIO_MMIO_STATUS,
                 status | VIRTIO_MMIO_STATUS_DRIVER_OK);
    __sync_synchronize();

    status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_MMIO_STATUS_DRIVER_OK))
        return TEST_SKIP;

    /* Reset by writing 0 */
    mmio_write32(dev, VIRTIO_MMIO_STATUS, 0);
    __sync_synchronize();

    /* Status must read 0 after reset */
    status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status != 0)
        TFAIL("status != 0");

    /* DeviceID must still be readable */
    uint32_t devid = mmio_read32(dev, VIRTIO_MMIO_DEVICE_ID);
    if (devid == 0)
        TFAIL("devid == 0");

    /* MagicValue must still be valid */
    uint32_t magic = mmio_read32(dev, VIRTIO_MMIO_MAGIC_VALUE);
    if (magic != VIRTIO_MMIO_MAGIC)
        TFAIL("magic != VIRTIO_MMIO_MAGIC");

    /* Re-init must work */
    mmio_write32(dev, VIRTIO_MMIO_STATUS, VIRTIO_MMIO_STATUS_ACKNOWLEDGE);
    __sync_synchronize();
    status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_MMIO_STATUS_ACKNOWLEDGE))
        TWEDGED("!(status & VIRTIO_MMIO_STATUS_ACKNOWLEDGE)");

    mmio_write32(dev, VIRTIO_MMIO_STATUS,
                 status | VIRTIO_MMIO_STATUS_DRIVER);
    __sync_synchronize();
    status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_MMIO_STATUS_DRIVER))
        TWEDGED("!(status & VIRTIO_MMIO_STATUS_DRIVER)");

    return TEST_PASS;
}

REGISTER_TEST_MMIO(M0010, do_test,
    "Device reset via Status=0 and re-init",
    VIRTIO_SPEC_V1_2, "4.2.3.1.1");
