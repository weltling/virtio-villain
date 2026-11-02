/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0017: pci_status_read_during_reset
 *
 * Write 0 to device_status (reset) and immediately read it back
 * in a tight loop. The spec says device_status reads 0 when reset
 * is complete. This races the reset path and tests whether the
 * device handles concurrent config space access during teardown.
 */
#include "lib/virtio_test.h"

static test_result_t test_status_read_during_reset(struct virtio_dev *dev)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /*
     * Trigger reset and immediately poll device_status.
     * The device must not crash during this race window.
     */
    cfg->device_status = 0;

    /* Tight poll loop - the spec says driver should read until 0 */
    int saw_zero = 0;
    for (int i = 0; i < 10000; i++) {
        uint8_t s = cfg->device_status;
        if (s == 0) {
            saw_zero = 1;
            break;
        }
    }

    if (!saw_zero) {
        /* Device didn't complete reset within 10000 reads */
        usleep(100000); /* 100ms grace */
        if (cfg->device_status != 0)
            TWEDGED("cfg->device_status != 0");
    }

    /*
     * Re-initialize to prove device is still functional after reset.
     */
    if (virtio_pci_init(dev) < 0)
        TREJECT("!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK)");

    return TEST_PASS;
}

REGISTER_TEST_DEV(PCI0017, VIRTIO_PCI_DEVICE_BLK, test_status_read_during_reset,
                  "Tight device_status polling during reset",
                  VIRTIO_SPEC_V1_2, "4.1.4.3.1");
