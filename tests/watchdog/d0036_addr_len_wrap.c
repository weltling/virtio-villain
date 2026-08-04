/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0036: watchdog writable descriptor whose addr plus len wraps 2^64.
 *
 * Sibling to D0006 and D0020, which keep the end address below 2^64.
 * Here the base sits near the top of the address space and the length
 * makes addr plus len wrap to a low value, so a device that computes the
 * end with a plain addition gets a small wrapped result and a naive
 * bounds check passes. The device must not access memory outside the
 * guest mapping or crash the VMM. Completing, silently rejecting, or
 * wedging the queue are all acceptable.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

static test_result_t test_watchdog_addr_len_wrap(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    vring_raw_set_desc(vr, 0, 0xFFFFFFFFFFFFF000ULL, 0x2000,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(D0036, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_addr_len_wrap,
              "Watchdog writable descriptor addr plus len wraps 64 bits",
              VIRTIO_SPEC_V1_2, "-");
