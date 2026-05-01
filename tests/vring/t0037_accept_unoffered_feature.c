/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0037: accept_unoffered_feature
 *
 * Acknowledge feature bits that the device did not offer. The device
 * should either fail FEATURES_OK or ignore the extra bits, not crash.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_accept_unoffered_feature(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    (void)vr;

    /* Reset and re-init with bogus features */
    virtio_pci_reset(dev);

    dev->common->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /* Read what the device offers */
    dev->common->device_feature_select = 0;
    __sync_synchronize();
    uint32_t offered_lo = dev->common->device_feature;
    dev->common->device_feature_select = 1;
    __sync_synchronize();
    uint32_t offered_hi = dev->common->device_feature;

    /*
     * Accept all offered features PLUS bits the device did NOT offer.
     * Flip all bits to 1.
     */
    dev->common->driver_feature_select = 0;
    dev->common->driver_feature = 0xFFFFFFFF;
    dev->common->driver_feature_select = 1;
    dev->common->driver_feature = 0xFFFFFFFF;
    __sync_synchronize();

    dev->common->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(10000);

    /*
     * The device may reject FEATURES_OK (clearing the bit) or accept.
     * Either is valid. The test passes as long as the VMM didn't crash.
     */
    (void)offered_lo;
    (void)offered_hi;

    /* Verify device is still alive by resetting and re-initing cleanly */
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
        vv_log("device unresponsive after unoffered feature negotiation");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    return TEST_PASS;
}

REGISTER_TEST(T0037, VIRTIO_PCI_DEVICE_BLK, test_accept_unoffered_feature,
              "Accept feature bits device did not offer",
              VIRTIO_SPEC_V1_2, "2.2");
