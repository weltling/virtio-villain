/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0004: Watchdog ping with zero-length descriptor.
 *
 * Submit a writable descriptor with zero length to the watchdog.
 * The device must handle the edge case without crashing.
 */
#include "lib/virtio_test.h"

static test_result_t test_watchdog_zero_len(struct virtio_dev *dev,
                                            struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    /* Zero-length writable descriptor */
    vv_desc(vr, 0, buf, 0, VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(D0004, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_zero_len,
              "Watchdog ping with zero-length descriptor",
              VIRTIO_SPEC_V1_2, "-");
