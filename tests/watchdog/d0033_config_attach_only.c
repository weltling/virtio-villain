/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0033: attach the watchdog and reach DRIVER_OK without touching
 * the device specific config region.
 *
 * Companion to D0032. The harness attaches the device, sets up all
 * queues and raises DRIVER_OK before this function runs, so this
 * test exercises the full bring up path yet never reads device cfg.
 *
 * Purpose: isolate device attach from the device cfg read. If this
 * passes but D0032 wedges, the wedge is the device cfg read itself,
 * not the attach. Skip if the device exposes no device cfg cap (the
 * region D0032 reads), so the two tests cover the same shape.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

static test_result_t test_watchdog_attach_only(
    struct virtio_dev *dev, struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)vr;

    if (!dev->device_cfg || dev->device_cfg_length == 0)
        return TEST_SKIP;

    __sync_synchronize();

    if (cfg->device_status == 0)
        TWEDGED("device_status reset to 0 after attach");
    return TEST_PASS;
}

REGISTER_TEST(D0033, VIRTIO_PCI_DEVICE_WATCHDOG,
              test_watchdog_attach_only,
              "Attach watchdog without reading device config",
              VIRTIO_SPEC_V1_4, "-");
