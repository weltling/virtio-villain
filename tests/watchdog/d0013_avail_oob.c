/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0013: Watchdog avail ring entry points past the descriptor table.
 *
 * Set avail->ring[0] to a head index well outside the descriptor
 * table. Per spec 2.7.6 the avail ring carries head indices into
 * the descriptor table; the device must reject the entry rather
 * than dereference an out of bounds slot.
 */
#include "lib/virtio_test.h"

static test_result_t test_watchdog_avail_oob(struct virtio_dev *dev,
                                             struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    /* Place a valid descriptor in slot 0 in case the device clamps. */
    vv_desc(vr, 0, buf, 1, VRING_DESC_F_WRITE, 0);

    /* But hand the device an out of range head index. */
    vring_raw_set_avail(vr, 0, 0xFFFE);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(D0013, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_avail_oob,
              "Watchdog avail entry past descriptor table",
              VIRTIO_SPEC_V1_2, "2.7.6");
