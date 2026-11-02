/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0005: Console transmit chain with self-loop.
 *
 * Build a TX chain whose head descriptor has VRING_DESC_F_NEXT and
 * points back at itself. The device must detect the cycle instead
 * of looping forever.
 */
#include "lib/virtio_test.h"

static test_result_t test_console_tx_self_loop(struct virtio_dev *dev,
                                               struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    vv_desc(vr, 0, buf, 1, VRING_DESC_F_NEXT, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(C0005, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_self_loop,
                "Console transmit chain self-loop",
                VIRTIO_SPEC_V1_2, "5.3", 1);
