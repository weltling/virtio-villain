/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0093: reserved status bits must be ignored.
 *
 * Spec 2.1: status bits 4, 5 and 7 (16, 32 and the high bit
 * of byte 0 above NEEDS_RESET=64) are reserved. Set them
 * alongside the running status; the device must not enter
 * FAILED and must keep accepting reads.
 */
#include "tests/test.h"

#include <unistd.h>

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint8_t saved = cfg->device_status;

    cfg->device_status = (uint8_t)(saved | 0x10u | 0x20u);
    __sync_synchronize();
    usleep(1000);

    uint8_t now = cfg->device_status;
    if (now & VIRTIO_STATUS_FAILED)
        TFAIL("device set FAILED in response to reserved bits");
    /* Best effort: device may ignore/preserve them. */
    return TEST_PASS;
}

REGISTER_TEST(S0093, VIRTIO_PCI_DEVICE_BLK, test,
              "Reserved status bits do not trigger FAILED",
              VIRTIO_SPEC_V1_4, "2.1");
