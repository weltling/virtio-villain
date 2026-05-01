/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0034: clear_status_bits
 *
 * Clear individual status bits (e.g. clear DRIVER while DRIVER_OK is
 * set). The spec says the driver MUST NOT clear a status bit. A VMM
 * that interprets the partial status as a state transition may crash.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_clear_status_bits(struct virtio_dev *dev,
                                            struct vring *vr)
{
    (void)vr;

    uint8_t original = dev->common->device_status;

    /* Clear DRIVER bit while keeping DRIVER_OK (illegal) */
    dev->common->device_status = original & ~VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    usleep(50000);

    /* Clear FEATURES_OK while keeping DRIVER_OK */
    dev->common->device_status = original & ~VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(50000);

    /* Write just ACKNOWLEDGE (clearing everything else) */
    dev->common->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    usleep(50000);

    /*
     * Try to re-init to verify device survived. Reset first.
     */
    virtio_pci_reset(dev);

    dev->common->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)) {
        vv_log("clearing status bits made device unresponsive");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    return TEST_PASS;
}

REGISTER_TEST(T0034, VIRTIO_PCI_DEVICE_BLK, test_clear_status_bits,
              "Clear individual status bits",
              VIRTIO_SPEC_V1_2, "3.1");
