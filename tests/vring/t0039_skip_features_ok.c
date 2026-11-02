/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0039: skip_features_ok
 *
 * Skip setting FEATURES_OK and go straight to queue setup and DRIVER_OK.
 * The spec says FEATURES_OK must be set before configuring queues.
 */
#include "lib/virtio_test.h"

static test_result_t test_skip_features_ok(struct virtio_dev *dev)
{
    /* Reset */
    virtio_pci_reset(dev);

    /* ACKNOWLEDGE + DRIVER but skip FEATURES_OK */
    dev->common->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /* Skip features negotiation entirely, go straight to DRIVER_OK */
    dev->common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Try to kick */
    virtio_pci_kick(dev, 0);
    usleep(200000);

    /*
     * Reset and verify the device survived.
     */
    virtio_pci_reset(dev);

    dev->common->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    dev->common->driver_feature_select = 0;
    dev->common->driver_feature = 0;
    dev->common->driver_feature_select = 1;
    dev->common->driver_feature = virtio_pci_base_features_hi(dev);
    dev->common->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)) {
        vv_log("skip FEATURES_OK made device unresponsive");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    return TEST_PASS;
}

REGISTER_TEST_DEV(T0039, VIRTIO_PCI_DEVICE_BLK, test_skip_features_ok,
                  "Skip FEATURES_OK, go straight to DRIVER_OK",
                  VIRTIO_SPEC_V1_2, "2.2");
