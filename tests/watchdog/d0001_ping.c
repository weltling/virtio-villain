/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0001: Watchdog ping (basic keepalive).
 *
 * Submit a write-only descriptor to the watchdog queue. The device
 * writes 1 into it and returns it, serving as a "ping" response.
 *
 * Cloud Hypervisor watchdog: single queue, driver submits writable
 * descriptors as keepalive pings.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_watchdog_ping(struct virtio_dev *dev,
                                        struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    *buf = 0;

    uint64_t buf_phys = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, buf_phys, 1, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(D0001, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_ping,
              "Watchdog ping keepalive",
              VIRTIO_SPEC_V1_2, "5.16");
