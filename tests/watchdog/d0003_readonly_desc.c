/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0003: Watchdog ping with read-only descriptor.
 *
 * Submit a non-writable descriptor to the watchdog queue. The device
 * expects writable descriptors for ping responses and must handle
 * this error gracefully.
 */
#include "lib/virtio_test.h"

static test_result_t test_watchdog_readonly(struct virtio_dev *dev,
                                            struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    *buf = 0;

    /* Descriptor without WRITE flag — device expects writable */
    vv_desc(vr, 0, buf, 1, 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(D0003, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_readonly,
              "Watchdog ping with non-writable descriptor",
              VIRTIO_SPEC_V1_2, "-");
