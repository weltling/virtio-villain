/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0043: queue_size_too_large
 *
 * Set queue_size larger than the maximum reported by the device.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_queue_size_too_large(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;

    virtio_pci_reset(dev);

    dev->common->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    dev->common->driver_feature_select = 0;
    dev->common->driver_feature = 0;
    dev->common->driver_feature_select = 1;
    dev->common->driver_feature = 0;
    dev->common->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    /* Read the device's max queue size */
    dev->common->queue_select = 0;
    __sync_synchronize();
    uint16_t max_size = dev->common->queue_size;

    /* Set to double the max (next power of 2 above max*2) */
    uint16_t huge = max_size ? (max_size << 1) : 32768;
    dev->common->queue_size = huge;
    __sync_synchronize();
    dev->common->queue_enable = 1;
    __sync_synchronize();

    dev->common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    virtio_pci_kick(dev, 0);
    usleep(200000);

    /* Reset and verify survival */
    virtio_pci_reset(dev);

    dev->common->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)) {
        vv_log("oversized queue_size made device unresponsive");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    return TEST_PASS;
}

REGISTER_TEST(T0043, VIRTIO_PCI_DEVICE_BLK, test_queue_size_too_large,
              "queue_size larger than device maximum",
              VIRTIO_SPEC_V1_2, "2.7");
