/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0016: RTC alarmq writable buffer (queue 1).
 *
 * The alarmq exists only when VIRTIO_RTC_F_ALARM is negotiated.
 * Post a writable buffer to queue 1 so the device can later use
 * it to deliver an alarm notification. Without F_ALARM queue 1
 * does not exist and the test will be skipped by the harness.
 */
#include "lib/virtio_test.h"

static test_result_t test_rtc_alarmq(struct virtio_dev *dev,
                                     struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    vv_desc(vr, 0, buf, 32, VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(RTC0016, VIRTIO_PCI_DEVICE_RTC, test_rtc_alarmq,
                "RTC alarmq writable buffer",
                VIRTIO_SPEC_V1_4, "5.23.6", 1);
