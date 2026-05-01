/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0036: reinit_without_reset
 *
 * Attempt to restart the initialization sequence (set ACKNOWLEDGE, then
 * DRIVER, etc.) without first writing 0 to device_status. The spec says
 * the driver MUST reset before re-initializing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_reinit_without_reset(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;

    /*
     * Device is currently in DRIVER_OK state.
     * Write ACKNOWLEDGE without resetting first (skip the status=0 step).
     */
    dev->common->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    usleep(10000);

    dev->common->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    usleep(10000);

    dev->common->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(10000);

    dev->common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Kick to stress the re-init path */
    virtio_pci_kick(dev, 0);
    usleep(200000);

    /*
     * Now do a proper reset and re-init to verify the device survived.
     */
    virtio_pci_reset(dev);

    dev->common->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)) {
        vv_log("reinit without reset made device unresponsive");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    return TEST_PASS;
}

REGISTER_TEST(T0036, VIRTIO_PCI_DEVICE_BLK, test_reinit_without_reset,
              "Re-init without resetting device first",
              VIRTIO_SPEC_V1_2, "3.1");
