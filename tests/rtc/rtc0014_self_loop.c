/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0014: RTC chain self-loop.
 *
 * Build a chain whose head sets NEXT and points back at itself.
 * The device must detect the cycle.
 */
#include "lib/virtio_test.h"

static test_result_t test_rtc_self_loop(struct virtio_dev *dev,
                                        struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    vv_desc(vr, 0, buf, 8, VRING_DESC_F_NEXT, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(RTC0014, VIRTIO_PCI_DEVICE_RTC, test_rtc_self_loop,
              "RTC chain self-loop",
              VIRTIO_SPEC_V1_4, "5.23.6");
