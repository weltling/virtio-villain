/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0032: watchdog device specific config region is reachable.
 *
 * Cloud Hypervisor virtio-watchdog (Device ID 35) has no spec
 * chapter and no device specific config fields. Read the
 * device cfg region (if present) and verify the read does not
 * wedge the device. If the device exposes no device cfg cap,
 * skip.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

static test_result_t test_watchdog_config_active_timeout(
    struct virtio_dev *dev, struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)vr;

    if (!dev->device_cfg || dev->device_cfg_length == 0)
        return TEST_SKIP;

    volatile uint8_t *p = dev->device_cfg;
    uint32_t sum = 0;
    for (uint32_t i = 0; i < dev->device_cfg_length; i++)
        sum += p[i];
    (void)sum;
    __sync_synchronize();

    if (cfg->device_status == 0)
        TWEDGED("device_status reset to 0 after config read");
    return TEST_PASS;
}

REGISTER_TEST(D0032, VIRTIO_PCI_DEVICE_WATCHDOG,
              test_watchdog_config_active_timeout,
              "Read entire watchdog device config region",
              VIRTIO_SPEC_V1_4, "-");
