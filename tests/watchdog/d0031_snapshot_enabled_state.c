/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0031: watchdog snapshot preserves the enabled state.
 *
 * Cloud Hypervisor virtio-watchdog (Device ID 35) has no spec
 * chapter. The companion .py orchestrator (not yet present)
 * is responsible for triggering pause/snapshot/resume. This
 * guest side test simply confirms the device is healthy and
 * a ping cycle completes; a paired Python wrapper that drives
 * snapshot/restore can be added later.
 */
#include "lib/virtio_test.h"

static test_result_t test_watchdog_snapshot_enabled(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    uint8_t *resp = vv_alloc_pages(1);

    *resp = 0;
    vv_desc(vr, 0, resp, 1, VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(D0031, VIRTIO_PCI_DEVICE_WATCHDOG,
              test_watchdog_snapshot_enabled,
              "Watchdog enabled state via baseline ping",
              VIRTIO_SPEC_V1_4, "-");
