/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0027: Watchdog ping with a one byte writable descriptor.
 *
 * The watchdog only requires a tiny pong buffer.
 * Submit a single byte writable descriptor. The device must
 * accept the minimal buffer and complete the ping without
 * underrunning or rounding the length up to a larger size.
 */
#include "lib/virtio_test.h"

static test_result_t test_watchdog_one_byte(struct virtio_dev *dev,
                                            struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    *buf = 0xA5;

    vv_desc(vr, 0, buf, 1, VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(D0027, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_one_byte,
              "Watchdog ping with a one byte writable buffer",
              VIRTIO_SPEC_V1_2, "-");
