/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0031: kick_after_reset
 *
 * Reset the device (status = 0) and then send a queue notification.
 * After reset, the device must not process any queue activity.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_kick_after_reset(struct virtio_dev *dev,
                                           struct vring *vr)
{
    (void)vr;

    /* Reset the device */
    virtio_pci_reset(dev);

    /* Kick queue 0 after reset */
    virtio_pci_kick(dev, 0);
    virtio_pci_kick(dev, 0);
    virtio_pci_kick(dev, 0);

    /* Give the VMM time to crash or panic */
    usleep(200000);

    /*
     * Re-initialize to verify the device is still responsive.
     */
    dev->common->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)) {
        vv_log("device not responsive after kick-after-reset");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    return TEST_PASS;
}

REGISTER_TEST(T0031, VIRTIO_PCI_DEVICE_BLK, test_kick_after_reset,
              "Queue kick after device reset",
              VIRTIO_SPEC_V1_2, "3.1");
