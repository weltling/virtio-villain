/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0040: SET_ALARM with an out-of-range clock id fires nothing.
 *
 * Spec 5.23.6: a SET_ALARM request naming a clock that does not
 * exist is invalid. The device must reject it and must never
 * deliver a NOTIF_ALARM for a clock it does not have. This test
 * posts a writable buffer on the alarmq, arms an enabled alarm for
 * clock id 0xFFFF on the requestq, then verifies the alarmq stays
 * empty and the device remains alive.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <stdint.h>
#include <unistd.h>

static test_result_t test_rtc_set_alarm_bad_clock(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1u << VIRTIO_RTC_F_ALARM)))
        return TEST_SKIP;
    if (cfg->num_queues < 2)
        return TEST_SKIP;

    /* Post a writable notification buffer on the alarmq (queue 1). */
    struct vring alarmq;
    vring_alloc(&alarmq, 16);
    vring_attach(dev, &alarmq, 1);

    struct rtc_notif_alarm *notif = vv_alloc_pages(1);
    memset(notif, 0, sizeof(*notif));
    vring_raw_set_desc(&alarmq, 0, vv_virt_to_phys(notif), sizeof(*notif),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&alarmq, 0, 0);
    vring_raw_set_avail_idx(&alarmq, 1);
    __sync_synchronize();
    virtio_pci_kick(dev, 1);

    /* Arm an enabled alarm for a clock that does not exist. */
    struct rtc_req_set_alarm_nested *req = vv_alloc_pages(1);
    struct rtc_resp_head *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    memset(resp, 0xFF, sizeof(*resp));
    req->head.msg_type = VIRTIO_RTC_REQ_SET_ALARM;
    req->clock_id = 0xFFFF;
    req->flags = VIRTIO_RTC_FLAG_ALARM_ENABLED;
    req->alarm_time = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r == TEST_WEDGED)
        return r;

    /* An invalid clock must never produce an alarm notification. */
    int waited = 0;
    while (waited < 100) {
        __sync_synchronize();
        if (alarmq.used->idx != 0)
            TFAIL("invalid clock alarm fired");
        usleep(1000);
        waited++;
    }

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(RTC0040, VIRTIO_PCI_DEVICE_RTC,
              test_rtc_set_alarm_bad_clock,
              "SET_ALARM with an out-of-range clock id fires nothing",
              VIRTIO_SPEC_V1_4, "5.23.6",
              0, 2);
