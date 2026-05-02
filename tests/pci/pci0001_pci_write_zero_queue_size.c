/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0001: pci_write_zero_queue_size
 *
 * Write 0 to the queue_size register via common_cfg. Spec 4.1.4.3.2:
 * driver MUST NOT write 0 to queue_size. A VMM may panic or divide
 * by zero when allocating queue structures.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_write_zero_queue_size(struct virtio_dev *dev,
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

    /* Write 0 to queue_size - spec violation */
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

    dev->common->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)) {
        vv_log("writing 0 to queue_size made device unresponsive");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    return TEST_PASS;
}

REGISTER_TEST(PCI0001, VIRTIO_PCI_DEVICE_BLK, test_pci_write_zero_queue_size,
              "Write 0 to PCI queue_size register",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
