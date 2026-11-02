/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0035: set_failed_then_kick
 *
 * Set the FAILED status bit and then kick a queue. Once FAILED is set,
 * the device should not process any more requests.
 */
#include "lib/virtio_test.h"

static test_result_t test_set_failed_then_kick(struct virtio_dev *dev)
{
    /* Set FAILED bit */
    dev->common->device_status |= VIRTIO_STATUS_FAILED;
    __sync_synchronize();
    usleep(10000);

    /* Kick after FAILED */
    virtio_pci_kick(dev, 0);
    virtio_pci_kick(dev, 0);

    usleep(200000);

    /*
     * Reset and re-init to verify device survived.
     */
    virtio_pci_reset(dev);

    if (virtio_pci_init(dev) < 0) {
        vv_log("kick after FAILED made device unresponsive");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    return TEST_PASS;
}

REGISTER_TEST_DEV(T0035, VIRTIO_PCI_DEVICE_BLK, test_set_failed_then_kick,
                  "Queue kick after setting FAILED status",
                  VIRTIO_SPEC_V1_2, "3.1");
