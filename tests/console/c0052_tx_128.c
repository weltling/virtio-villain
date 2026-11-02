/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0052: console TX 128 bytes.
 *
 * Submit 128 bytes on the TX queue. Tests moderate size transmit.
 */
#include "lib/virtio_test.h"

static test_result_t test_console_tx_128(struct virtio_dev *dev,
                                         struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 'A', 128);

    vv_desc(vr, 0, buf, 128, 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(C0052, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_128,
              "TX 128 bytes",
              VIRTIO_SPEC_V1_2, "5.3.6", 1);
