/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0105: device_status is 0 after reset.
 *
 * Spec 2.1: The device status field starts at 0 and resets to 0.
 * After a fresh device init, read device_status and verify it was
 * initialized to 0 before the harness wrote ACKNOWLEDGE.
 *
 * Note: the harness already wrote ACKNOWLEDGE by the time the test
 * runs, so we reset and check the zero state explicitly.
 */
#include "lib/virtio_test.h"

static test_result_t test_pci_status_init(struct virtio_dev *dev)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Reset device */
    cfg->device_status = 0;
    __sync_synchronize();

    /* Poll until status reads 0 (reset complete) */
    int waited = 0;
    while (waited < VV_TIMEOUT_MS) {
        __sync_synchronize();
        if (cfg->device_status == 0)
            break;
        usleep(1000);
        waited++;
    }

    uint8_t st = cfg->device_status;
    if (st != 0)
        TFAIL("device_status %u after reset, expected 0", st);

    /* Restore for clean teardown */
    if (virtio_pci_init(dev) < 0)
        TFAIL("base feature negotiation rejected after reset");
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    return TEST_PASS;
}

REGISTER_TEST_DEV(PCI0105, VIRTIO_PCI_DEVICE_BLK, test_pci_status_init,
                  "Device status is 0 after reset",
                  VIRTIO_SPEC_V1_2, "2.1");
