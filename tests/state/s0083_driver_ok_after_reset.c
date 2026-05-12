/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0083: driver_ok_immediately_after_reset
 *
 * Reset the device and write DRIVER_OK as the very next status
 * value, skipping ACKNOWLEDGE, DRIVER, feature negotiation, and
 * queue setup entirely. Spec 3.1.1 requires steps 1 through 7
 * before step 8. This is the most extreme sequence violation.
 * The device must not crash or corrupt host memory.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_driver_ok_after_reset(struct virtio_dev *dev,
                                                struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    /* Reset */
    virtio_pci_reset(dev);

    /* Write DRIVER_OK directly, skipping everything */
    cfg->device_status = VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    usleep(100000);

    /* Device must still be alive */
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(S0083, VIRTIO_PCI_DEVICE_BLK, test_driver_ok_after_reset,
              "DRIVER_OK immediately after reset",
              VIRTIO_SPEC_V1_2, "3.1.1");
