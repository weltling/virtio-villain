/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0020: rtc_alarm_time_zero
 *
 * Submit an alarmq SET request whose time_ns is zero. Spec draft
 * RTC.7 says the device must behave deterministically for an
 * alarm time in the past. The test passes if the device responds
 * (either fires or rejects) and stays alive across two repeats.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rtc_alarm_zero(struct virtio_dev *dev,
                                         struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    if (cfg->num_queues < 2)
        return TEST_SKIP;

    struct vring alarmq;
    vring_alloc(&alarmq, 16);
    vring_attach(dev, &alarmq, 1);

    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 256);
    struct rtc_req_set_alarm_flat *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_SET_ALARM;
    req->alarm_id = 0;
    req->time_ns = 0;

    uint64_t base = vv_virt_to_phys(buf);
    vring_raw_set_desc(&alarmq, 0, base, sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&alarmq, 1, base + 64, sizeof(struct rtc_resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&alarmq, 0, 0);
    vring_raw_set_avail_idx(&alarmq, 1);

    test_result_t r = vv_kick_and_wait(dev, &alarmq, 1, VV_TIMEOUT_MS);
    if (r == TEST_WEDGED)
        return r;

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    return r;
}

REGISTER_TEST_REQUIRES(RTC0020, VIRTIO_PCI_DEVICE_RTC, test_rtc_alarm_zero,
              "SET_ALARM with time_ns equal to zero",
              VIRTIO_SPEC_V1_4, "RTC.7",
              0, 2);
