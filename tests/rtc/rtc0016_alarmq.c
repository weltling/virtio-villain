/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0016: RTC alarmq writable buffer (queue 1).
 *
 * The alarmq exists only when VIRTIO_RTC_F_ALARM is negotiated.
 * Post a writable buffer to queue 1 so the device can later use
 * it to deliver an alarm notification. Without F_ALARM queue 1
 * does not exist and the test will be skipped by the harness.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <stdint.h>

static test_result_t test_rtc_alarmq(struct virtio_dev *dev,
                                     struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 64);
    uint64_t base = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, base, 32, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(RTC0016, VIRTIO_PCI_DEVICE_RTC, test_rtc_alarmq,
                "RTC alarmq writable buffer",
                VIRTIO_SPEC_V1_4, "5.23.6", 1);
