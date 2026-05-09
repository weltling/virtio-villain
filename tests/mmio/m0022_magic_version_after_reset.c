/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0022: read Magic and Version after Status reset (spec 4.2.2.2)
 *
 * Reset the device by writing Status=0, then read Magic and
 * Version. Both registers are read only and must keep their
 * canonical values. DeviceID and VendorID must also remain
 * stable.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    uint32_t magic_before = mmio_read32(dev, VIRTIO_MMIO_MAGIC_VALUE);
    uint32_t version_before = mmio_read32(dev, VIRTIO_MMIO_VERSION);
    uint32_t devid_before = mmio_read32(dev, VIRTIO_MMIO_DEVICE_ID);
    uint32_t vendid_before = mmio_read32(dev, VIRTIO_MMIO_VENDOR_ID);

    mmio_write32(dev, VIRTIO_MMIO_STATUS, 0);
    __sync_synchronize();

    if (mmio_read32(dev, VIRTIO_MMIO_MAGIC_VALUE) != magic_before)
        TFAIL("mmio_read32(dev, VIRTIO_MMIO_MAGIC_VALUE) != magic_before");
    if (mmio_read32(dev, VIRTIO_MMIO_VERSION) != version_before)
        TFAIL("mmio_read32(dev, VIRTIO_MMIO_VERSION) != version_before");
    if (mmio_read32(dev, VIRTIO_MMIO_DEVICE_ID) != devid_before)
        TFAIL("mmio_read32(dev, VIRTIO_MMIO_DEVICE_ID) != devid_before");
    if (mmio_read32(dev, VIRTIO_MMIO_VENDOR_ID) != vendid_before)
        TFAIL("mmio_read32(dev, VIRTIO_MMIO_VENDOR_ID) != vendid_before");

    if (magic_before != VIRTIO_MMIO_MAGIC)
        TFAIL("magic_before != VIRTIO_MMIO_MAGIC");

    return TEST_PASS;
}

REGISTER_TEST_MMIO(M0022, do_test,
    "Magic and Version stable after Status reset",
    VIRTIO_SPEC_V1_2, "4.2.2.2");
