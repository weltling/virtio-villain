/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0019: Write to device config (read only for driver).
 *
 * The watchdog config space is device writable only.
 * The driver should not write it. Attempt a write and confirm the
 * device does not crash or change state unexpectedly.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_watchdog_config_write(struct virtio_dev *dev,
                                                struct vring *vr)
{
    (void)vr;

    if (dev->device_cfg_length < 4)
        return TEST_SKIP;

    volatile uint32_t *timer = (volatile uint32_t *)dev->device_cfg;
    *timer = 0xDEADBEEF;
    __sync_synchronize();

    usleep(100000);

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST(D0019, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_config_write,
              "Write to read only config space",
              VIRTIO_SPEC_V1_2, "-");
