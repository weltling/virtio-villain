/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0026: Config change notification racing with reset (spec 4.1.4.3.2)
 *
 * Rapidly alternate between reading config space and resetting.
 * Tests that config change notifications or reads don't corrupt
 * device state during concurrent reset.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_config_change_reset_race(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    for (int i = 0; i < 20; i++) {
        /* Read config generation */
        uint8_t gen = cfg->config_generation;
        (void)gen;

        /* Read device config if available */
        if (dev->device_cfg && dev->device_cfg_length >= 8) {
            volatile uint64_t cap = *(volatile uint64_t *)dev->device_cfg;
            (void)cap;
        }

        /* Reset */
        cfg->device_status = 0;
        __sync_synchronize();

        /* Don't wait for reset to complete - immediately read config */
        if (dev->device_cfg && dev->device_cfg_length >= 8) {
            volatile uint64_t cap = *(volatile uint64_t *)dev->device_cfg;
            (void)cap;
        }

        gen = cfg->config_generation;
        (void)gen;

        /* Quick re-init */
        int tries = 50;
        while (tries-- > 0 && cfg->device_status != 0)
            usleep(1000);

        cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
        __sync_synchronize();

        cfg->driver_feature_select = 0;
        cfg->driver_feature = 0;
        __sync_synchronize();

        cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
        __sync_synchronize();
        usleep(2000);

        if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
            continue; /* device rejected, try again */

        cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
        __sync_synchronize();
    }

    /* Final check: device should still be alive */
    uint8_t status = cfg->device_status;
    if (status == 0)
        TWEDGED("status == 0");

    return TEST_PASS;
}

REGISTER_TEST(S0026, VIRTIO_PCI_DEVICE_BLK, test_config_change_reset_race,
              "Config space reads racing with device resets",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
