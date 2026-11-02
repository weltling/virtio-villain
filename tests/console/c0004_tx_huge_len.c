/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0004: Console transmit with oversized length.
 *
 * Submit a transmitq descriptor whose length field claims a multi-MB
 * payload while the underlying allocation is only one page. The
 * device must validate the length against guest memory bounds and
 * must not read past the end of the buffer.
 */
#include "lib/virtio_test.h"

static test_result_t test_console_tx_huge_len(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    /* Length 1 GiB while only 4 KiB is mapped */
    vv_desc(vr, 0, buf, 0x40000000U, 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(C0004, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_huge_len,
                "Console transmit with 1 GiB length",
                VIRTIO_SPEC_V1_2, "5.3", 1);
