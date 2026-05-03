/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0017: Version and Magic validation (spec 4.2.2.1)
 *
 * Verify MagicValue reads 0x74726976 ("virt") and Version reads 2
 * for a modern MMIO device. These are the minimum checks a driver
 * should perform before proceeding with initialization.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    /* Read MagicValue without any init */
    uint32_t magic = mmio_read32(dev, VIRTIO_MMIO_MAGIC_VALUE);
    if (magic != VIRTIO_MMIO_MAGIC)
        TFAIL("magic != VIRTIO_MMIO_MAGIC");

    /* Read Version - must be 2 for modern devices */
    uint32_t version = mmio_read32(dev, VIRTIO_MMIO_VERSION);
    if (version != 2)
        TFAIL("version != 2");

    /* DeviceID must be non-zero for a valid device */
    uint32_t devid = mmio_read32(dev, VIRTIO_MMIO_DEVICE_ID);
    if (devid == 0)
        TFAIL("devid == 0");

    /* VendorID should be non-zero */
    uint32_t vendid = mmio_read32(dev, VIRTIO_MMIO_VENDOR_ID);
    if (vendid == 0)
        TFAIL("vendid == 0");

    /* Read all these multiple times to verify stability */
    for (int i = 0; i < 100; i++) {
        if (mmio_read32(dev, VIRTIO_MMIO_MAGIC_VALUE) != magic)
            TFAIL("mmio_read32(dev, VIRTIO_MMIO_MAGIC_VALUE) != magic");
        if (mmio_read32(dev, VIRTIO_MMIO_VERSION) != version)
            TFAIL("mmio_read32(dev, VIRTIO_MMIO_VERSION) != version");
        if (mmio_read32(dev, VIRTIO_MMIO_DEVICE_ID) != devid)
            TFAIL("mmio_read32(dev, VIRTIO_MMIO_DEVICE_ID) != devid");
    }

    return TEST_PASS;
}

REGISTER_TEST_MMIO(M0017, do_test,
    "Verify MagicValue=0x74726976, Version=2, DeviceID!=0",
    VIRTIO_SPEC_V1_2, "4.2.2.1");
