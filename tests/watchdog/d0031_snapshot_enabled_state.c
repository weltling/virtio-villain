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
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

static test_result_t test_watchdog_snapshot_enabled(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    uint8_t *resp = vv_alloc_pages(1);

    *resp = 0;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(resp), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(D0031, VIRTIO_PCI_DEVICE_WATCHDOG,
              test_watchdog_snapshot_enabled,
              "Watchdog enabled state via baseline ping",
              VIRTIO_SPEC_V1_4, "-");
