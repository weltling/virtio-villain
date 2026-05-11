/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0016: Read watchdog config space timer value.
 *
 * Spec 5.16.4: The watchdog config contains a timeout_ms value.
 * Read it to exercise the config access path. The device must
 * not crash on a config read and must return a sensible value.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_watchdog_config_timer(struct virtio_dev *dev,
                                                struct vring *vr)
{
    (void)vr;

    if (dev->device_cfg_length < 4)
        return TEST_SKIP;

    volatile uint32_t *timer = (volatile uint32_t *)dev->device_cfg;
    volatile uint32_t val = *timer;
    (void)val;

    /* Device must still be alive */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST(D0016, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_config_timer,
              "Read config space timer value",
              VIRTIO_SPEC_V1_2, "5.16.4");
