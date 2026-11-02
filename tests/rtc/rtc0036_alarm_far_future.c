/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0036: SET_ALARM far in the future does not fire immediately.
 *
 * Spec 5.23.6: when VIRTIO_RTC_F_ALARM is negotiated a SET_ALARM
 * request with FLAG_ALARM_ENABLED and an alarm_time that lies in
 * the past fires at once, while an alarm_time in the future must
 * not fire until that time is reached. This complements the
 * immediate fire path by arming an alarm far ahead and checking
 * that the alarmq stays empty for a bounded window.
 *
 * Post a writable buffer on the alarmq, arm an alarm with a large
 * future alarm_time on the requestq, wait for the requestq
 * response, then verify the alarmq used ring does not advance.
 */
#include "lib/virtio_test.h"

static test_result_t test_rtc_alarm_far_future(struct virtio_dev *dev,
                                               struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    if (!virtio_pci_feature_offered(dev, VIRTIO_RTC_F_ALARM))
        return TEST_SKIP;
    if (cfg->num_queues < 2)
        return TEST_SKIP;

    /* Post a writable notification buffer on the alarmq (queue 1). */
    struct vring alarmq;
    vring_alloc(&alarmq, 16);
    vring_attach(dev, &alarmq, 1);

    struct rtc_notif_alarm *notif = vv_alloc_pages(1);
    vv_desc(&alarmq, 0, notif, sizeof(*notif), VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&alarmq, 0, 0);
    vring_raw_set_avail_idx(&alarmq, 1);
    __sync_synchronize();
    virtio_pci_kick(dev, 1);

    /* Arm an alarm far in the future on the requestq. */
    struct rtc_req_set_alarm_nested *req = vv_alloc_pages(1);
    struct rtc_resp_head *resp = vv_alloc_pages(1);
    memset(resp, 0xFF, sizeof(*resp));
    req->head.msg_type = VIRTIO_RTC_REQ_SET_ALARM;
    req->clock_id = 0;
    req->flags = VIRTIO_RTC_FLAG_ALARM_ENABLED;
    req->alarm_time = (uint64_t)1 << 62;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    VV_TRY(vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS));
    if (resp->status == VIRTIO_RTC_S_ENODEV)
        return TEST_SKIP;
    if (resp->status != VIRTIO_RTC_S_OK)
        return TEST_SKIP;

    /* The alarmq must stay empty for a bounded window. */
    int waited = 0;
    while (waited < 100) {
        __sync_synchronize();
        if (alarmq.used->idx != 0)
            TFAIL("far future alarm fired immediately");
        usleep(1000);
        waited++;
    }

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(RTC0036, VIRTIO_PCI_DEVICE_RTC,
              test_rtc_alarm_far_future,
              "SET_ALARM far in the future does not fire immediately",
              VIRTIO_SPEC_V1_4, "5.23.6",
              0, 2);
