/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0074: suspend_needs_reset_on_failure
 *
 * The device MUST set DEVICE_NEEDS_RESET if it fails to suspend.
 * This test sets SUSPEND and checks the device either suspends
 * successfully or sets NEEDS_RESET as the alternative outcome.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_suspend_needs_reset(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)vr;

    /* Check if SUSPEND is offered */
    cfg->device_feature_select = VIRTIO_F_SUSPEND / 32;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << (VIRTIO_F_SUSPEND % 32))))
        return TEST_SKIP;

    /* Set SUSPEND */
    uint8_t status = cfg->device_status;
    cfg->device_status = status | VIRTIO_STATUS_SUSPEND;
    __sync_synchronize();
    usleep(300000);

    uint8_t new_status = cfg->device_status;

    /* Acceptable outcomes: either suspended or NEEDS_RESET */
    if (new_status & VIRTIO_STATUS_SUSPEND) {
        /* Successfully suspended */
        return TEST_PASS;
    }
    if (new_status & VIRTIO_STATUS_NEEDS_RESET) {
        /* Device indicated failure via NEEDS_RESET */
        return TEST_PASS;
    }

    if (new_status == 0)
        TWEDGED("new_status == 0");

    /* Neither suspended nor NEEDS_RESET is a spec violation */
    TFAIL("unexpected fallthrough");
}

REGISTER_TEST(S0074, VIRTIO_PCI_DEVICE_BLK, test_suspend_needs_reset,
              "Suspend either succeeds or sets DEVICE_NEEDS_RESET",
              VIRTIO_SPEC_V1_3, "3.2");
