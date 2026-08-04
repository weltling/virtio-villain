/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0055: console tx readable descriptor addr plus len wraps 2^64.
 *
 * Sibling to C0004, which claims an oversized length with the end below
 * 2^64. Here the transmit descriptor base sits near the top of the
 * address space and the length makes addr plus len wrap to a low value.
 * The descriptor is device readable, so this exercises the device read
 * from guest memory rather than the write path. The device must not read
 * outside the guest mapping or crash the VMM. Completing, silently
 * rejecting, or wedging the queue are all acceptable.
 *
 * Spec 5.3.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_console_tx_addr_len_wrap(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    vring_raw_set_desc(vr, 0, 0xFFFFFFFFFFFFF000ULL, 0x2000, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(C0055, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_addr_len_wrap,
                "Console tx readable descriptor addr plus len wraps 64 bits",
                VIRTIO_SPEC_V1_2, "5.3", 1);
