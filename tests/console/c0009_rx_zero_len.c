/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0009: Console receive with zero-length writable descriptor.
 *
 * Submit a writable descriptor of length 0 to the receiveq. The
 * device must not write past the (non-existent) buffer when host
 * input arrives. On a quiet console the expected outcome is
 * REJECT (no completion observed within the timeout).
 */
#include "lib/virtio_test.h"

static test_result_t test_console_rx_zero_len(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    vv_desc(vr, 0, buf, 0, VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(C0009, VIRTIO_PCI_DEVICE_CONSOLE, test_console_rx_zero_len,
              "Console receive zero-length writable descriptor",
              VIRTIO_SPEC_V1_2, "5.3");
