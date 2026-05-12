/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0082: driver_ok_without_features_ok
 *
 * Reset the device, set ACKNOWLEDGE and DRIVER, then jump straight
 * to DRIVER_OK without setting FEATURES_OK. Spec 3.1.1 step 6
 * says the driver must set FEATURES_OK and verify it sticks before
 * proceeding. Skipping it means the device activates with
 * acked_features in an undefined state. The device must not crash;
 * it may degrade to features=0.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_driver_ok_skip_features_ok(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    /* Reset */
    virtio_pci_reset(dev);

    /* ACKNOWLEDGE + DRIVER */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /* Skip FEATURES_OK, go straight to DRIVER_OK */
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    usleep(100000);

    /* Device must still be alive */
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(S0082, VIRTIO_PCI_DEVICE_BLK, test_driver_ok_skip_features_ok,
              "DRIVER_OK without prior FEATURES_OK",
              VIRTIO_SPEC_V1_2, "3.1.1");
