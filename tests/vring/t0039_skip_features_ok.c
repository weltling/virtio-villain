/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0039: skip_features_ok
 *
 * Skip setting FEATURES_OK and go straight to queue setup and DRIVER_OK.
 * The spec says FEATURES_OK must be set before configuring queues.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_skip_features_ok(struct virtio_dev *dev,
                                           struct vring *vr)
{
    (void)vr;

    /* Reset */
    virtio_pci_reset(dev);

    /* ACKNOWLEDGE + DRIVER but skip FEATURES_OK */
    dev->common->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /* Skip features negotiation entirely, go straight to DRIVER_OK */
    dev->common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Try to kick */
    virtio_pci_kick(dev, 0);
    usleep(200000);

    /*
     * Reset and verify the device survived.
     */
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

    if (!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)) {
        vv_log("skip FEATURES_OK made device unresponsive");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    return TEST_PASS;
}

REGISTER_TEST(T0039, VIRTIO_PCI_DEVICE_BLK, test_skip_features_ok,
              "Skip FEATURES_OK, go straight to DRIVER_OK",
              VIRTIO_SPEC_V1_2, "2.2");
