/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0045: queue_avail_unaligned
 *
 * Set queue_avail to an odd address (must be 2-byte aligned).
 */
#include "lib/virtio_test.h"

static test_result_t test_queue_avail_unaligned(struct virtio_dev *dev)
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

    void *page = vv_alloc_pages(1);
    uint64_t phys = vv_virt_to_phys(page);

    dev->common->queue_select = 0;
    __sync_synchronize();
    dev->common->queue_size = 16;
    virtio_store64(&dev->common->queue_desc, phys);
    /* Set avail to odd address */
    virtio_store64(&dev->common->queue_avail, phys + 257);
    virtio_store64(&dev->common->queue_used, phys + 512);
    dev->common->queue_enable = 1;
    __sync_synchronize();

    dev->common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    virtio_pci_kick(dev, 0);
    usleep(200000);

    virtio_pci_reset(dev);

    if (virtio_pci_init(dev) < 0) {
        vv_log("unaligned queue_avail made device unresponsive");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    return TEST_PASS;
}

REGISTER_TEST_DEV(T0045, VIRTIO_PCI_DEVICE_BLK, test_queue_avail_unaligned,
                  "queue_avail address not 2-byte aligned",
                  VIRTIO_SPEC_V1_2, "2.7");
