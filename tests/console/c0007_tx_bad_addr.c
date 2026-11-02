/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0007: Console transmit with bogus physical address.
 *
 * Submit a TX descriptor whose addr field points to an address
 * outside any valid guest memory region (top of 64-bit space). The
 * device must fail the read attempt safely rather than pass an
 * unchecked address into the host's translation path.
 */
#include "lib/virtio_test.h"

static test_result_t test_console_tx_bad_addr(struct virtio_dev *dev,
                                              struct vring *vr)
{
    /* High address, well above any plausible guest RAM */
    vring_raw_set_desc(vr, 0, 0xFFFFFFFFFFFF0000ULL, 16, 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(C0007, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_bad_addr,
                "Console transmit with out-of-range address",
                VIRTIO_SPEC_V1_2, "5.3", 1);
