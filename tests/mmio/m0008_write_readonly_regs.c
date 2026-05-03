/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0008: Write to read-only MMIO registers (spec 4.2.2.2)
 *
 * MagicValue, Version, DeviceID, VendorID, DeviceFeatures,
 * QueueNumMax, InterruptStatus, and ConfigGeneration are read-only.
 * Writing to them must not crash the VMM.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    if (virtio_mmio_init(dev) < 0)
        return TEST_SKIP;

    /* Save original values */
    uint32_t magic = mmio_read32(dev, VIRTIO_MMIO_MAGIC_VALUE);
    uint32_t version = mmio_read32(dev, VIRTIO_MMIO_VERSION);
    uint32_t devid = mmio_read32(dev, VIRTIO_MMIO_DEVICE_ID);
    uint32_t vendid = mmio_read32(dev, VIRTIO_MMIO_VENDOR_ID);

    /* Write garbage to read-only registers */
    mmio_write32(dev, VIRTIO_MMIO_MAGIC_VALUE, 0xDEADBEEF);
    mmio_write32(dev, VIRTIO_MMIO_VERSION, 0xFFFFFFFF);
    mmio_write32(dev, VIRTIO_MMIO_DEVICE_ID, 0x12345678);
    mmio_write32(dev, VIRTIO_MMIO_VENDOR_ID, 0xCAFEBABE);

    /* Write to QueueNumMax (read-only) */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_SEL, 0);
    __sync_synchronize();
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_NUM_MAX, 0xFFFF);

    /* Write to InterruptStatus (read-only, not InterruptACK) */
    mmio_write32(dev, VIRTIO_MMIO_INTERRUPT_STATUS, 0xFFFFFFFF);

    /* Write to ConfigGeneration (read-only) */
    mmio_write32(dev, VIRTIO_MMIO_CONFIG_GENERATION, 0xFFFFFFFF);

    /* Write to DeviceFeatures (read-only, driver uses DriverFeatures) */
    mmio_write32(dev, VIRTIO_MMIO_DEVICE_FEATURES, 0xFFFFFFFF);

    /* Verify device is still alive */
    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status == 0)
        TWEDGED("status == 0");

    /* Verify read-only registers were not corrupted */
    if (mmio_read32(dev, VIRTIO_MMIO_MAGIC_VALUE) != magic)
        TFAIL("mmio_read32(dev, VIRTIO_MMIO_MAGIC_VALUE) != magic");
    if (mmio_read32(dev, VIRTIO_MMIO_VERSION) != version)
        TFAIL("mmio_read32(dev, VIRTIO_MMIO_VERSION) != version");
    if (mmio_read32(dev, VIRTIO_MMIO_DEVICE_ID) != devid)
        TFAIL("mmio_read32(dev, VIRTIO_MMIO_DEVICE_ID) != devid");
    if (mmio_read32(dev, VIRTIO_MMIO_VENDOR_ID) != vendid)
        TFAIL("mmio_read32(dev, VIRTIO_MMIO_VENDOR_ID) != vendid");

    TREJECT("no device response within timeout");
}

REGISTER_TEST_MMIO(M0008, do_test,
    "Write to read-only MMIO registers",
    VIRTIO_SPEC_V1_2, "4.2.2.2");
