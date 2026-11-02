/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0070: suspend_without_feature
 *
 * Set SUSPEND bit without having negotiated VIRTIO_F_SUSPEND.
 * The device should ignore the bit as it is undefined without
 * the feature.
 */
#include "lib/virtio_test.h"

static test_result_t test_suspend_no_feature(struct virtio_dev *dev)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* The harness does not negotiate SUSPEND. */
    uint8_t status = cfg->device_status;
    cfg->device_status = status | VIRTIO_STATUS_SUSPEND;
    __sync_synchronize();
    usleep(100000);

    /* Device should not suspend (DRIVER_OK must remain set) */
    uint8_t new_status = cfg->device_status;
    if (new_status == 0)
        TWEDGED("new_status == 0");
    if (!(new_status & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("!(new_status & VIRTIO_STATUS_DRIVER_OK)");

    return TEST_PASS;
}

REGISTER_TEST_DEV(S0070, VIRTIO_PCI_DEVICE_BLK, test_suspend_no_feature,
                  "Set SUSPEND bit without VIRTIO_F_SUSPEND negotiated",
                  VIRTIO_SPEC_V1_3, "3.2");
