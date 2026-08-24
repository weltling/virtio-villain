/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0048: queue_addr_outside_ram
 *
 * Set queue_desc to an address far beyond guest RAM.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_queue_addr_outside_ram(struct virtio_dev *dev,
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

    dev->common->queue_select = 0;
    __sync_synchronize();
    dev->common->queue_size = 16;
    /* All addresses beyond guest RAM */
    virtio_store64(&dev->common->queue_desc, 0xFFFFFFFFFFFF0000ULL);
    virtio_store64(&dev->common->queue_avail, 0xFFFFFFFFFFFF1000ULL);
    virtio_store64(&dev->common->queue_used, 0xFFFFFFFFFFFF2000ULL);
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
        vv_log("queue address beyond RAM made device unresponsive");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    return TEST_PASS;
}

REGISTER_TEST(T0048, VIRTIO_PCI_DEVICE_BLK, test_queue_addr_outside_ram,
              "queue_desc/avail/used beyond guest RAM",
              VIRTIO_SPEC_V1_2, "2.7");
