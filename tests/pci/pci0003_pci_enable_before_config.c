/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0003: pci_enable_queue_before_config
 *
 * Set queue_enable=1 before writing queue_size or addresses.
 * Spec 4.1.4.3.2: driver MUST configure queue before enabling.
 * The VMM may use stale/default values.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_enable_before_config(struct virtio_dev *dev,
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

    /* Enable queue BEFORE setting size or addresses */
    dev->common->queue_enable = 1;
    __sync_synchronize();

    /* Now set config after the fact */
    dev->common->queue_size = 16;
    struct vring vr2;
    vring_alloc(&vr2, 16);
    virtio_store64(&dev->common->queue_desc, vr2.desc_phys);
    virtio_store64(&dev->common->queue_avail, vr2.avail_phys);
    virtio_store64(&dev->common->queue_used, vr2.used_phys);
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
        vv_log("enable before config made device unresponsive");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    return TEST_PASS;
}

REGISTER_TEST(PCI0003, VIRTIO_PCI_DEVICE_BLK, test_pci_enable_before_config,
              "Enable queue before configuring via PCI cap",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
