/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0011: Watchdog ping with all reserved descriptor flag bits set.
 *
 * Submit a ping descriptor with VRING_DESC_F_WRITE plus every
 * reserved bit in the flags field set. Per spec 2.7.5 reserved
 * bits MUST be zero; the device must either ignore the unknown
 * bits or reject the descriptor cleanly.
 */
#include "lib/virtio_test.h"

static test_result_t test_watchdog_all_flags(struct virtio_dev *dev,
                                             struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    /* WRITE bit plus every reserved bit in the 16 bit flags field. */
    uint16_t flags = (uint16_t)(VRING_DESC_F_WRITE | 0xFFF8);

    vv_desc(vr, 0, buf, 1, flags, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(D0011, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_all_flags,
              "Watchdog ping with all reserved flags set",
              VIRTIO_SPEC_V1_2, "2.7.5");
