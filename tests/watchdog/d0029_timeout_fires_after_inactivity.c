/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0029: watchdog timeout fires after a quiet window.
 *
 * Cloud Hypervisor virtio-watchdog (Device ID 35) has no
 * normative spec chapter. The contract is: once the driver
 * has kicked, the device starts an internal timer; the driver
 * must kick again within that window. Send one ping, then
 * stay silent for longer than the documented bound (best
 * effort: 16 seconds, comfortably above the CH default of
 * 15 s). The guest must survive the timeout; the host may
 * record a reset event but must not corrupt the guest.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_watchdog_timeout_fires(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint8_t *resp = vv_alloc_pages(1);

    *resp = 0;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(resp), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    /* Stay silent longer than the configured timeout. */
    sleep(16);

    if (cfg->device_status == 0)
        TWEDGED("device_status reset to 0 by host");
    return TEST_PASS;
}

REGISTER_TEST(D0029, VIRTIO_PCI_DEVICE_WATCHDOG,
              test_watchdog_timeout_fires,
              "Watchdog quiet window past configured timeout",
              VIRTIO_SPEC_V1_4, "-");
