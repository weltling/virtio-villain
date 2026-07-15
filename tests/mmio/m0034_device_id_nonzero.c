/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0034: MMIO DeviceID register is non zero.
 *
 * Spec 4.2.2.1: The DeviceID register at offset 0x008 identifies
 * the device type. A value of 0 means no device is present at this
 * MMIO location. Verify it is non zero for a discovered device.
 */
#include "tests/test.h"
#include "lib/virtio_mmio.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    uint32_t device_id = mmio_read32(dev, VIRTIO_MMIO_DEVICE_ID);

    if (device_id == 0)
        TFAIL("DeviceID is 0 (no device present)");

    return TEST_PASS;
}

REGISTER_TEST_MMIO(M0034, do_test,
    "MMIO DeviceID is non zero",
    VIRTIO_SPEC_V1_2, "4.2.2.1");
