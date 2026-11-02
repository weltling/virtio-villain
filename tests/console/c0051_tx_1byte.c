/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0051: console TX single byte.
 *
 * Submit one byte on the TX queue. Tests the minimum valid
 * transmit buffer size.
 */
#include "lib/virtio_test.h"

static test_result_t test_console_tx_1byte(struct virtio_dev *dev,
                                           struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    buf[0] = 'X';

    vv_desc(vr, 0, buf, 1, 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(C0051, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_1byte,
              "TX single byte",
              VIRTIO_SPEC_V1_2, "5.3.6", 1);
