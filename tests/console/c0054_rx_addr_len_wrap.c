/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0054: console rx writable descriptor whose addr plus len wraps 2^64.
 *
 * Siblings C0010 and C0037 keep the end address below 2^64. Here the
 * base sits near the top of the address space and the length makes addr
 * plus len wrap to a low value, so a device that computes the end with a
 * plain addition gets a small wrapped result and a naive bounds check
 * passes. The device must not access memory outside the guest mapping or
 * crash the VMM. Completing, silently rejecting, or wedging the queue are
 * all acceptable.
 *
 * Spec 5.3.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_console_rx_addr_len_wrap(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    vring_raw_set_desc(vr, 0, 0xFFFFFFFFFFFFF000ULL, 0x2000,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(C0054, VIRTIO_PCI_DEVICE_CONSOLE, test_console_rx_addr_len_wrap,
              "Console rx writable descriptor addr plus len wraps 64 bits",
              VIRTIO_SPEC_V1_2, "5.3");
