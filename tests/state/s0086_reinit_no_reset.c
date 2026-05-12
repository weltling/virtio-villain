/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0086: reinit_without_reset
 *
 * After reaching DRIVER_OK, attempt to restart the initialization
 * sequence (write ACKNOWLEDGE) without first resetting the device
 * (writing 0). Spec 3.1.1 step 1 says "Reset the device" is the
 * first step of initialization. Restarting without reset is a
 * protocol violation. The device must not crash or enter an
 * inconsistent state.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_reinit_without_reset(struct virtio_dev *dev,
                                               struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    /* Verify we are in DRIVER_OK from init */
    uint8_t old = cfg->device_status;
    if (!(old & VIRTIO_STATUS_DRIVER_OK))
        return TEST_SKIP;

    /* Attempt to restart init without resetting first */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    usleep(50000);

    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    usleep(50000);

    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(50000);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    usleep(100000);

    /* Device must still be alive */
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(S0086, VIRTIO_PCI_DEVICE_BLK, test_reinit_without_reset,
              "Restart init sequence without prior reset",
              VIRTIO_SPEC_V1_2, "3.1.1");
