/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0084: features_ok_without_negotiation
 *
 * Reset the device, set ACKNOWLEDGE and DRIVER, then immediately
 * set FEATURES_OK without writing any driver_feature value.
 * Spec 3.1.1 step 4 says the driver should read device features
 * and write the subset it accepts. Skipping negotiation means
 * acked_features stays at zero. The device must accept or reject
 * FEATURES_OK but not crash.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_features_ok_no_negotiation(struct virtio_dev *dev,
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

    /* Skip feature reads/writes, go straight to FEATURES_OK */
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(50000);

    /* Read back; device may clear FEATURES_OK */
    uint8_t st = cfg->device_status;

    if (st == 0)
        TWEDGED("st == 0");

    /*
     * Either FEATURES_OK sticks (device accepts zero features)
     * or it was cleared (device rejected). Both are valid.
     */
    (void)st;
    return TEST_PASS;
}

REGISTER_TEST(S0084, VIRTIO_PCI_DEVICE_BLK, test_features_ok_no_negotiation,
              "FEATURES_OK without any feature negotiation",
              VIRTIO_SPEC_V1_2, "3.1.1");
