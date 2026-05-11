/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0035: console_tx_zero_length_buffer
 *
 * Submit a transmit buffer with length zero on the TX queue.
 * Spec 5.3.6.1 says the driver sends data via the TX queue.
 * A zero length buffer is a degenerate case that must not crash
 * or confuse the console output path.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_console_tx_zero(struct virtio_dev *dev,
                                          struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 0, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    /* TX is queue 1 for console */
    return vv_kick_and_wait(dev, vr, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(C0035, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_zero,
                "TX buffer with zero length",
                VIRTIO_SPEC_V1_2, "5.3.6.1", 1);
