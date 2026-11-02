/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0006: pci_msix_vector_oob
 *
 * Write queue_msix_vector to a value outside the MSI-X table size.
 * Spec 4.1.5.1.2.2: driver SHOULD use valid MSI-X vector or NO_VECTOR.
 * Writing an invalid vector may cause VMM to index out of bounds.
 */
#include "lib/virtio_test.h"

static test_result_t test_pci_msix_vector_oob(struct virtio_dev *dev)
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

    /* Set msix_config to an absurdly high vector */
    dev->common->msix_config = 0xFFFE;
    __sync_synchronize();

    /* Set queue msix_vector to OOB value (not NO_VECTOR=0xFFFF) */
    dev->common->queue_msix_vector = 0xFFFE;
    __sync_synchronize();

    dev->common->queue_size = 16;
    struct vring vr2;
    vring_alloc(&vr2, 16);
    virtio_store64(&dev->common->queue_desc, vr2.desc_phys);
    virtio_store64(&dev->common->queue_avail, vr2.avail_phys);
    virtio_store64(&dev->common->queue_used, vr2.used_phys);
    dev->common->queue_enable = 1;
    __sync_synchronize();

    dev->common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    virtio_pci_kick(dev, 0);
    usleep(200000);

    /* Reset and verify survival */
    virtio_pci_reset(dev);

    if (virtio_pci_init(dev) < 0) {
        vv_log("OOB MSI-X vector made device unresponsive");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    return TEST_PASS;
}

REGISTER_TEST_DEV(PCI0006, VIRTIO_PCI_DEVICE_BLK, test_pci_msix_vector_oob,
                  "MSI-X vector outside table bounds",
                  VIRTIO_SPEC_V1_2, "4.1.5.1.2.2");
