/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0046: set DRIVER before ACKNOWLEDGE
 *
 * Spec 2.1.2 prescribes ACKNOWLEDGE then DRIVER then features then
 * FEATURES_OK then DRIVER_OK. A driver that writes DRIVER first
 * without ACKNOWLEDGE skips a step. The device must not crash on
 * this. A clean reset and proper reinit must still work.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_status_wrong_order(struct virtio_dev *dev,
                                             struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    /* Reset */
    virtio_pci_reset(dev);

    /* Wrong order: DRIVER alone */
    cfg->device_status = VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    usleep(5000);

    /* Reset and verify still works */
    virtio_pci_reset(dev);

    /* Proper reinit */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    if (!(cfg->device_status & VIRTIO_STATUS_DRIVER))
        TFAIL("!(cfg->device_status & VIRTIO_STATUS_DRIVER)");

    return TEST_PASS;
}

REGISTER_TEST(S0046, VIRTIO_PCI_DEVICE_BLK, test_status_wrong_order,
              "DRIVER without ACKNOWLEDGE then reset cleanly",
              VIRTIO_SPEC_V1_2, "2.1.2");
