/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0012: Console receive chain with self-loop.
 *
 * Build an RX chain whose head sets VRING_DESC_F_NEXT|WRITE and
 * points back at itself. The device must detect the cycle instead
 * of looping forever following the next pointer when host input
 * arrives.
 */
#include "lib/virtio_test.h"

static test_result_t test_console_rx_self_loop(struct virtio_dev *dev,
                                               struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    vv_desc(vr, 0, buf, 32, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(C0012, VIRTIO_PCI_DEVICE_CONSOLE, test_console_rx_self_loop,
              "Console receive chain self-loop",
              VIRTIO_SPEC_V1_2, "5.3");
