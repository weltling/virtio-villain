/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0037: pings across a span longer than the timeout keep the guest.
 *
 * Cloud Hypervisor virtio-watchdog (Device ID 35) has no spec chapter.
 * Each accepted ping resets the internal deadline. D0030 spaces three
 * pings a few seconds apart, all inside one timeout window, so it never
 * crosses the deadline. Here the pings are 8 seconds apart across 16
 * seconds, past the CH default 15 second timeout. Without a working per
 * ping reset the watchdog would fire mid sequence; the guest must
 * instead stay alive through every ping. The total stays within the
 * per test wall clock budget that D0029 already relies on.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define PING_COUNT 3
#define PING_GAP_S 8

static test_result_t test_watchdog_keepalive_across_timeout(
    struct virtio_dev *dev, struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint8_t *resp = vv_alloc_pages(1);

    for (uint16_t i = 0; i < PING_COUNT; i++) {
        *resp = 0;
        vring_raw_set_desc(vr, i, vv_virt_to_phys(resp), 1,
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, i, i);
        vring_raw_set_avail_idx(vr, (uint16_t)(i + 1));

        test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
        if (r != TEST_PASS)
            return r;

        if (cfg->device_status == 0)
            TWEDGED("device_status reset to 0 mid keepalive");

        if (i + 1 < PING_COUNT)
            sleep(PING_GAP_S);
    }

    if (cfg->device_status == 0)
        TWEDGED("device_status reset to 0 after keepalive");
    return TEST_PASS;
}

REGISTER_TEST(D0037, VIRTIO_PCI_DEVICE_WATCHDOG,
              test_watchdog_keepalive_across_timeout,
              "Pings past the timeout window keep the guest alive",
              VIRTIO_SPEC_V1_4, "-");
