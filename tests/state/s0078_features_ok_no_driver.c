/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0078: status_ack_without_driver
 *
 * Set device_status to FEATURES_OK without first setting DRIVER.
 * Spec 3.1.1 step 3 says the driver must set DRIVER before
 * proceeding to feature negotiation. Skipping the step is a
 * protocol violation the device must tolerate.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_status_skip_driver(struct virtio_dev *dev,
                                             struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    virtio_pci_reset(dev);

    /* ACKNOWLEDGE only, skip DRIVER */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();

    /* Jump straight to FEATURES_OK */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE |
                         VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    /* Read back; device may clear FEATURES_OK or ignore */
    uint8_t st = cfg->device_status;
    (void)st;

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST(S0078, VIRTIO_PCI_DEVICE_BLK, test_status_skip_driver,
              "FEATURES_OK without prior DRIVER bit",
              VIRTIO_SPEC_V1_2, "3.1.1");
