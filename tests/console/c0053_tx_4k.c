/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0053: console TX 4096 bytes (full page).
 *
 * Submit 4096 bytes on the TX queue. Tests that the console
 * device handles a full page transmit buffer.
 */
#include "lib/virtio_test.h"

static test_result_t test_console_tx_4k(struct virtio_dev *dev,
                                        struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 'Z', 4096);

    vv_desc(vr, 0, buf, 4096, 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(C0053, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_4k,
              "TX 4096 bytes (full page)",
              VIRTIO_SPEC_V1_2, "5.3.6", 1);
