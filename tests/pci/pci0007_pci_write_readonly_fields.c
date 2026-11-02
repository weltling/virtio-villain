/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0007: pci_write_readonly_fields
 *
 * Write to fields in the common_cfg that are read-only from the driver
 * perspective: device_feature_select/device_feature (reading device
 * features is allowed, but the device_feature register itself is
 * device-writable only), num_queues, config_generation.
 * Spec 4.1.4.8.1: these fields are read-only for the driver.
 */
#include "lib/virtio_test.h"

static test_result_t test_pci_write_readonly_fields(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    (void)vr;

    /*
     * Write to read-only fields. The device should ignore writes
     * or handle them gracefully without crashing.
     */

    /* num_queues is read-only */
    volatile uint16_t orig_num_queues = dev->common->num_queues;
    dev->common->num_queues = 0;
    __sync_synchronize();
    dev->common->num_queues = 0xFFFF;
    __sync_synchronize();

    /* config_generation is read-only */
    dev->common->config_generation = 0xFF;
    __sync_synchronize();

    /* device_feature is read-only (reflects device's offered features) */
    dev->common->device_feature_select = 0;
    __sync_synchronize();
    dev->common->device_feature = 0xFFFFFFFF;
    __sync_synchronize();

    /* Verify the device is still operational */
    uint16_t nq = dev->common->num_queues;
    if (nq == 0) {
        vv_log("write to num_queues actually took effect (now 0)");
        TFAIL("nq == 0");
    }

    /* Reset and verify survival */
    virtio_pci_reset(dev);

    if (virtio_pci_init(dev) < 0) {
        vv_log("writing read-only fields made device unresponsive");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    (void)orig_num_queues;
    return TEST_PASS;
}

REGISTER_TEST(PCI0007, VIRTIO_PCI_DEVICE_BLK, test_pci_write_readonly_fields,
              "Write to read-only PCI capability fields",
              VIRTIO_SPEC_V1_2, "4.1.4.8.1");
