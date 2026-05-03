/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0011: DeviceFeaturesSel / DriverFeaturesSel out of range (spec 4.2.2.2)
 *
 * The spec defines feature bits in two 32-bit pages (sel=0, sel=1).
 * Setting sel to large values (2, 0xFFFFFFFF) is out of range.
 * The device must handle this gracefully.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    /* Reset and start init */
    mmio_write32(dev, VIRTIO_MMIO_STATUS, 0);
    __sync_synchronize();
    mmio_write32(dev, VIRTIO_MMIO_STATUS, VIRTIO_MMIO_STATUS_ACKNOWLEDGE);
    __sync_synchronize();
    mmio_write32(dev, VIRTIO_MMIO_STATUS,
                 VIRTIO_MMIO_STATUS_ACKNOWLEDGE | VIRTIO_MMIO_STATUS_DRIVER);
    __sync_synchronize();

    /* Read features at valid sel=0 */
    mmio_write32(dev, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 0);
    __sync_synchronize();
    volatile uint32_t feat0 = mmio_read32(dev, VIRTIO_MMIO_DEVICE_FEATURES);
    (void)feat0;

    /* Read features at valid sel=1 */
    mmio_write32(dev, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 1);
    __sync_synchronize();
    volatile uint32_t feat1 = mmio_read32(dev, VIRTIO_MMIO_DEVICE_FEATURES);
    (void)feat1;

    /* Read features at invalid sel=2 (should return 0 or garbage, not crash) */
    mmio_write32(dev, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 2);
    __sync_synchronize();
    volatile uint32_t feat2 = mmio_read32(dev, VIRTIO_MMIO_DEVICE_FEATURES);
    (void)feat2;

    /* Read features at invalid sel=0xFFFFFFFF */
    mmio_write32(dev, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 0xFFFFFFFF);
    __sync_synchronize();
    volatile uint32_t featmax = mmio_read32(dev, VIRTIO_MMIO_DEVICE_FEATURES);
    (void)featmax;

    /* Write driver features at invalid sel values */
    mmio_write32(dev, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 2);
    __sync_synchronize();
    mmio_write32(dev, VIRTIO_MMIO_DRIVER_FEATURES, 0xFFFFFFFF);

    mmio_write32(dev, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0xFFFFFFFF);
    __sync_synchronize();
    mmio_write32(dev, VIRTIO_MMIO_DRIVER_FEATURES, 0xFFFFFFFF);

    /* Verify device still alive */
    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status == 0)
        TWEDGED("status == 0");

    TREJECT("no device response within timeout");
}

REGISTER_TEST_MMIO(M0011, do_test,
    "FeaturesSel set to out-of-range values",
    VIRTIO_SPEC_V1_2, "4.2.2.2");
