/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0042: queue_size_zero
 *
 * Set queue_size to 0 and try to enable the queue. A queue of size 0
 * has no descriptors and should be rejected.
 */
#include "lib/virtio_test.h"

static test_result_t test_queue_size_zero(struct virtio_dev *dev)
{
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

    dev->common->queue_select = 0;
    __sync_synchronize();
    dev->common->queue_size = 0;
    __sync_synchronize();
    dev->common->queue_enable = 1;
    __sync_synchronize();

    dev->common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    virtio_pci_kick(dev, 0);
    usleep(200000);

    /* Reset and verify survival */
    virtio_pci_reset(dev);

    if (virtio_pci_init(dev) < 0) {
        vv_log("queue_size=0 made device unresponsive");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    return TEST_PASS;
}

REGISTER_TEST_DEV(T0042, VIRTIO_PCI_DEVICE_BLK, test_queue_size_zero,
                  "queue_size set to 0",
                  VIRTIO_SPEC_V1_2, "2.7");
