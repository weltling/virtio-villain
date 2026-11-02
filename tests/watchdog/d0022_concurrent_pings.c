/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0022: Watchdog concurrent pings.
 *
 * Submit two watchdog pings in the same avail ring
 * batch. The device must handle multiple in flight pings without
 * confusion or double completion.
 */
#include "lib/virtio_test.h"

static test_result_t test_watchdog_concurrent_pings(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    uint8_t *buf1 = vv_alloc_pages(1);
    uint8_t *buf2 = vv_alloc_pages(1);

    vv_desc(vr, 0, buf1, 1, VRING_DESC_F_WRITE, 0);
    vv_desc(vr, 1, buf2, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 1);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(D0022, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_concurrent_pings,
              "Two watchdog pings submitted in one batch",
              VIRTIO_SPEC_V1_2, "-");
