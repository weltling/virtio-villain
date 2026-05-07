/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0042: rapid config_generation polling
 *
 * Spec 4.1.4.3.1 defines config_generation as a counter the device
 * increments whenever a config field changes. The driver must read
 * the counter, read the config, then re-read the counter to detect
 * a torn read. Hammering the register at high rate while no config
 * changes are happening must keep the value stable and the device
 * alive. This catches VMMs that touch device state on every read.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_config_generation_hammer(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    uint8_t first = cfg->config_generation;
    __sync_synchronize();

    for (int i = 0; i < 10000; i++) {
        uint8_t v = cfg->config_generation;
        if (v != first)
            TFAIL("v != first");
    }

    if (cfg->device_status == 0)
        TFAIL("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(S0042, VIRTIO_PCI_DEVICE_BLK, test_config_generation_hammer,
              "10000 config_generation reads stay stable when idle",
              VIRTIO_SPEC_V1_2, "4.1.4.3.1");
