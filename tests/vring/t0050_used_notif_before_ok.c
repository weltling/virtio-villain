/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0050: used_notification_before_driver_ok
 *
 * Write to the used ring event notification area before setting
 * DRIVER_OK. The spec says the driver MUST NOT use the device
 * before setting DRIVER_OK. A VMM that processes notifications
 * before the device is active may use uninitialized state.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_used_notif_before_ok(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;

    /* Reset device */
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

    /* Set up queue normally */
    dev->common->queue_select = 0;
    __sync_synchronize();

    struct vring vr2;
    vring_alloc(&vr2, 16);
    vring_attach(dev, &vr2, 0);

    /* Write avail_event in used ring BEFORE DRIVER_OK */
    vr2.used->idx = 0;
    __sync_synchronize();

    /* Kick before DRIVER_OK */
    virtio_pci_kick(dev, 0);
    usleep(200000);

    /* Now set DRIVER_OK and verify device is alive */
    dev->common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    usleep(100000);

    /* Reset and check survival */
    virtio_pci_reset(dev);

    dev->common->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    dev->common->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)) {
        vv_log("used notification before DRIVER_OK broke device");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    return TEST_PASS;
}

REGISTER_TEST(T0050, VIRTIO_PCI_DEVICE_BLK, test_used_notif_before_ok,
              "Used ring notification before DRIVER_OK",
              VIRTIO_SPEC_V1_2, "3.1.1");
