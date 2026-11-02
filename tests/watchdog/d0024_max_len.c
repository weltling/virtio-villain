/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0024: Watchdog ping with maximum length descriptor.
 *
 * Submit a ping with len set to 0xFFFFFFFF. Even
 * though the watchdog only needs a tiny buffer, the device must
 * not overflow internal calculations when presented with a
 * maximum length writable descriptor.
 */
#include "lib/virtio_test.h"

static test_result_t test_watchdog_max_len(struct virtio_dev *dev,
                                           struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    vv_desc(vr, 0, buf, 0xFFFFFFFFU, VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(D0024, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_max_len,
              "Watchdog ping with maximum descriptor length",
              VIRTIO_SPEC_V1_2, "-");
