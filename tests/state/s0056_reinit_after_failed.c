/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0056: reinit after the driver sets FAILED then resets
 *
 * Spec 2.1.2 says if the driver sets the FAILED bit it indicates
 * giving up on the device. The device must still accept a reset
 * that follows and return to status zero, and a fresh reinit
 * sequence must complete successfully. Set FAILED, then reset,
 * then walk the device through ACKNOWLEDGE then DRIVER then
 * FEATURES_OK and verify each transition takes effect.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_reinit_after_failed(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    /* Set FAILED bit on top of current status */
    cfg->device_status |= VIRTIO_STATUS_FAILED;
    __sync_synchronize();
    if (!(cfg->device_status & VIRTIO_STATUS_FAILED))
        TFAIL("!(cfg->device_status & VIRTIO_STATUS_FAILED)");

    /* Reset must still take effect */
    virtio_pci_reset(dev);

    /* Full reinit walk */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    if (!(cfg->device_status & VIRTIO_STATUS_ACKNOWLEDGE))
        TFAIL("!(cfg->device_status & VIRTIO_STATUS_ACKNOWLEDGE)");

    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    if (!(cfg->device_status & VIRTIO_STATUS_DRIVER))
        TFAIL("!(cfg->device_status & VIRTIO_STATUS_DRIVER)");

    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = 0;
    __sync_synchronize();

    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        TFAIL("!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK)");

    return TEST_PASS;
}

REGISTER_TEST(S0056, VIRTIO_PCI_DEVICE_BLK, test_reinit_after_failed,
              "reset and reinit succeed after FAILED bit was set",
              VIRTIO_SPEC_V1_2, "2.1.2");
