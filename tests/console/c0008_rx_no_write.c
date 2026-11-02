/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0008: Console receive descriptor without WRITE flag.
 *
 * Submit a receiveq (queue 0) descriptor without VRING_DESC_F_WRITE.
 * The device fills the receiveq when host-side input arrives. A
 * read-only descriptor on the RX path is a driver bug. The device
 * must not write past the buffer or crash. Because the result here
 * depends on whether the host pushes any input, REJECT (silent) is
 * the expected outcome on a quiet console.
 */
#include "lib/virtio_test.h"

static test_result_t test_console_rx_no_write(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    /* RX queue, but descriptor not marked writable */
    vv_desc(vr, 0, buf, 64, 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(C0008, VIRTIO_PCI_DEVICE_CONSOLE, test_console_rx_no_write,
              "Console receive descriptor without WRITE flag",
              VIRTIO_SPEC_V1_2, "5.3");
