/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0041: queue_size_not_power_of_two
 *
 * Set queue_size to 17 (not a power of two). The spec says queue_size
 * MUST be a power of 2 for split virtqueues.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_queue_size_not_power_of_two(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    (void)vr;

    /* Reset */
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

    /* Select queue 0 and set non-power-of-two size */
    dev->common->queue_select = 0;
    __sync_synchronize();
    dev->common->queue_size = 17;
    __sync_synchronize();

    /* Try to enable the queue */
    dev->common->queue_enable = 1;
    __sync_synchronize();
    usleep(50000);

    /* Kick to force the VMM to process the bad config */
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
        vv_log("non-power-of-two queue_size made device unresponsive");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    return TEST_PASS;
}

REGISTER_TEST(T0041, VIRTIO_PCI_DEVICE_BLK, test_queue_size_not_power_of_two,
              "queue_size set to non-power-of-two (17)",
              VIRTIO_SPEC_V1_2, "2.7");
