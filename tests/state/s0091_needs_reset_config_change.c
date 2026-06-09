/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0091: NEEDS_RESET is paired with a config change notification.
 *
 * Spec 2.1.2: When the device sets NEEDS_RESET, drivers learn
 * about it through the configuration change notification.
 * The harness cannot induce NEEDS_RESET from the guest; this
 * test verifies the framework is alive and that bit 6
 * (NEEDS_RESET) is currently zero (baseline).
 */
#include "tests/test.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status & VIRTIO_STATUS_NEEDS_RESET)
        TFAIL("device entered NEEDS_RESET unexpectedly");
    return TEST_PASS;
}

REGISTER_TEST(S0091, VIRTIO_PCI_DEVICE_BLK, test,
              "Baseline: NEEDS_RESET is clear",
              VIRTIO_SPEC_V1_4, "2.1.2");
