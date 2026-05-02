/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0005: pci_notify_disabled_queue
 *
 * Send a queue notification (kick) for a queue that was never enabled.
 * Spec 4.1.4.9.2: driver MUST NOT notify a disabled queue.
 * The VMM may index into an uninitialized array.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_notify_disabled_queue(struct virtio_dev *dev,
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

    /* Select queue 0 but do NOT enable it */
    dev->common->queue_select = 0;
    __sync_synchronize();
    dev->common->queue_size = 16;
    __sync_synchronize();

    /* Set DRIVER_OK without enabling the queue */
    dev->common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Notify a queue that is not enabled */
    virtio_pci_kick(dev, 0);
    usleep(200000);

    /* Also try notifying a queue index that likely doesn't exist */
    virtio_pci_kick(dev, 15);
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
        vv_log("notify on disabled queue made device unresponsive");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    return TEST_PASS;
}

REGISTER_TEST(PCI0005, VIRTIO_PCI_DEVICE_BLK, test_pci_notify_disabled_queue,
              "Notify a queue that was never enabled",
              VIRTIO_SPEC_V1_2, "4.1.4.9.2");
