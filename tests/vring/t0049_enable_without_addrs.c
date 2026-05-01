/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0049: enable_without_addrs
 *
 * Set queue_enable = 1 without first setting queue_desc/avail/used
 * addresses. The VMM may dereference NULL or uninitialized pointers.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_enable_without_addrs(struct virtio_dev *dev,
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

    /* Select queue 0, set size, but do NOT set addresses */
    dev->common->queue_select = 0;
    __sync_synchronize();
    dev->common->queue_size = 16;
    /* Leave queue_desc, queue_avail, queue_used at 0 */
    dev->common->queue_desc = 0;
    dev->common->queue_avail = 0;
    dev->common->queue_used = 0;
    dev->common->queue_enable = 1;
    __sync_synchronize();

    dev->common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    virtio_pci_kick(dev, 0);
    usleep(200000);

    virtio_pci_reset(dev);

    dev->common->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)) {
        vv_log("enable without addresses made device unresponsive");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    return TEST_PASS;
}

REGISTER_TEST(T0049, VIRTIO_PCI_DEVICE_BLK, test_enable_without_addrs,
              "queue_enable without setting addresses",
              VIRTIO_SPEC_V1_2, "2.7");
