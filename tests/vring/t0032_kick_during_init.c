/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0032: kick_during_init
 *
 * Send a queue notification while the device is only in ACKNOWLEDGE
 * state (no DRIVER, no DRIVER_OK). The device must ignore it.
 */
#include "lib/virtio_test.h"

static test_result_t test_kick_during_init(struct virtio_dev *dev)
{
    /* Reset and set only ACKNOWLEDGE */
    virtio_pci_reset(dev);

    dev->common->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();

    /* Kick while only ACKNOWLEDGE is set */
    virtio_pci_kick(dev, 0);
    virtio_pci_kick(dev, 0);

    usleep(200000);

    /* Continue init to verify device survived */
    dev->common->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    dev->common->driver_feature_select = 0;
    dev->common->driver_feature = 0;
    dev->common->driver_feature_select = 1;
    dev->common->driver_feature = virtio_pci_base_features_hi(dev);
    dev->common->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)) {
        vv_log("device not responsive after kick during init");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    return TEST_PASS;
}

REGISTER_TEST_DEV(T0032, VIRTIO_PCI_DEVICE_BLK, test_kick_during_init,
                  "Queue kick during ACKNOWLEDGE-only state",
                  VIRTIO_SPEC_V1_2, "3.1");
