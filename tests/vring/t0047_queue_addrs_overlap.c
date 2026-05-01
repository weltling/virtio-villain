/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0047: queue_addrs_overlap
 *
 * Set queue_desc, queue_avail, and queue_used to the same address so
 * they overlap in guest memory. The VMM must detect or handle this.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_queue_addrs_overlap(struct virtio_dev *dev,
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

    void *page = vv_alloc_pages(1);
    uint64_t phys = vv_virt_to_phys(page);

    dev->common->queue_select = 0;
    __sync_synchronize();
    dev->common->queue_size = 16;
    /* All three point to the same address */
    dev->common->queue_desc = phys;
    dev->common->queue_avail = phys;
    dev->common->queue_used = phys;
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
        vv_log("overlapping queue addresses made device unresponsive");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    return TEST_PASS;
}

REGISTER_TEST(T0047, VIRTIO_PCI_DEVICE_BLK, test_queue_addrs_overlap,
              "queue_desc/avail/used all at same address",
              VIRTIO_SPEC_V1_2, "2.7");
