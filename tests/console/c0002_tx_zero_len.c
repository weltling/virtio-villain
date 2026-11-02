/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0002: Console transmit with zero-length descriptor.
 *
 * Submit a read-only descriptor of length 0 to the transmitq. The
 * device must handle the empty payload edge case without crashing.
 */
#include "lib/virtio_test.h"

static test_result_t test_console_tx_zero_len(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    vv_desc(vr, 0, buf, 0, 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(C0002, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_zero_len,
                "Console transmit zero-length descriptor",
                VIRTIO_SPEC_V1_2, "5.3", 1);
