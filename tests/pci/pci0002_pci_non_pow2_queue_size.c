/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0002: pci_write_non_power_of_two_queue_size
 *
 * Write a non-power-of-two value (7) to queue_size via PCI common_cfg.
 * Spec 4.1.4.3.2: driver MUST use a power of 2 for queue_size.
 * Different from T41 which uses 17 - this uses a smaller odd value.
 */
#include "lib/virtio_test.h"

static test_result_t test_pci_non_pow2_queue_size(struct virtio_dev *dev)
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

    /* Write 7 (not a power of 2) via PCI register */
    dev->common->queue_size = 7;
    __sync_synchronize();

    struct vring vr2;
    vring_alloc(&vr2, 7);
    virtio_store64(&dev->common->queue_desc, vr2.desc_phys);
    virtio_store64(&dev->common->queue_avail, vr2.avail_phys);
    virtio_store64(&dev->common->queue_used, vr2.used_phys);
    dev->common->queue_enable = 1;
    __sync_synchronize();

    dev->common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    virtio_pci_kick(dev, 0);
    usleep(200000);

    virtio_pci_reset(dev);

    if (virtio_pci_init(dev) < 0) {
        vv_log("non-power-of-two queue_size (7) made device unresponsive");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    return TEST_PASS;
}

REGISTER_TEST_DEV(PCI0002, VIRTIO_PCI_DEVICE_BLK, test_pci_non_pow2_queue_size,
                  "Write non-power-of-two (7) to PCI queue_size",
                  VIRTIO_SPEC_V1_2, "4.1.4.3.2");
