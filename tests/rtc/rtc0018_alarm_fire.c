/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0018: alarm SET then NOTIF_ALARM delivered on alarmq.
 *
 * Spec RTC.5 says when VIRTIO_RTC_F_ALARM is negotiated the
 * driver may post a SET_ALARM request with FLAG_ALARM_ENABLED
 * and a past or zero alarm_time. The device must consider this
 * an immediate alarm expiration event and deliver a single
 * NOTIF_ALARM through the alarmq, with msg_type set to
 * VIRTIO_RTC_NOTIF_ALARM and clock_id set to the expired clock.
 *
 * The test reinits the device with two queues, posts a writable
 * buffer on the alarmq, sends SET_ALARM on the requestq with
 * alarm_time zero and the enabled flag, waits for the requestq
 * response, then waits for the alarmq used ring to advance and
 * verifies the notification message type and clock id.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <stdint.h>
#include <unistd.h>

static test_result_t test_rtc_alarm_fire(struct virtio_dev *dev,
                                         struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    if (cfg->num_queues < 2)
        return TEST_SKIP;

    (void)vr;

    virtio_pci_reset(dev);
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t feat0 = cfg->device_feature;
    if (!(feat0 & (1u << VIRTIO_RTC_F_ALARM)))
        return TEST_SKIP;

    cfg->driver_feature_select = 0;
    cfg->driver_feature = feat0;
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        return TEST_SKIP;

    struct vring rq, aq;
    vring_alloc(&rq, 16);
    vring_alloc(&aq, 16);
    vring_attach(dev, &rq, 0);
    vring_attach(dev, &aq, 1);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Post writable buffer on alarmq */
    struct rtc_notif_alarm *notif = vv_alloc_pages(1);
    memset(notif, 0, sizeof(*notif));
    vring_raw_set_desc(&aq, 0, vv_virt_to_phys(notif), sizeof(*notif),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&aq, 0, 0);
    vring_raw_set_avail_idx(&aq, 1);
    __sync_synchronize();
    virtio_pci_kick(dev, 1);

    /* Build SET_ALARM on requestq */
    struct rtc_req_set_alarm_nested *req = vv_alloc_pages(1);
    struct rtc_resp_head *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    memset(resp, 0xFF, sizeof(*resp));
    req->head.msg_type = VIRTIO_RTC_REQ_SET_ALARM;
    req->alarm_time = 0;
    req->clock_id = 0;
    req->flags = VIRTIO_RTC_FLAG_ALARM_ENABLED;

    vring_raw_set_desc(&rq, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&rq, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&rq, 0, 0);
    vring_raw_set_avail_idx(&rq, 1);
    __sync_synchronize();
    virtio_pci_kick(dev, 0);

    int waited = 0;
    while (waited < VV_TIMEOUT_MS) {
        __sync_synchronize();
        if (rq.used->idx != 0)
            break;
        usleep(1000);
        waited++;
    }
    if (rq.used->idx == 0)
        TWEDGED("rq.used->idx == 0");

    if (resp->status == VIRTIO_RTC_S_ENODEV)
        return TEST_SKIP;
    if (resp->status != VIRTIO_RTC_S_OK)
        TFAIL("resp->status != VIRTIO_RTC_S_OK");

    /* Now wait for NOTIF_ALARM on alarmq */
    waited = 0;
    while (waited < VV_TIMEOUT_MS) {
        __sync_synchronize();
        if (aq.used->idx != 0)
            break;
        usleep(1000);
        waited++;
    }
    if (aq.used->idx == 0)
        TREJECT("aq.used->idx == 0");

    if (notif->msg_type != VIRTIO_RTC_NOTIF_ALARM)
        TFAIL("notif->msg_type != VIRTIO_RTC_NOTIF_ALARM");
    if (notif->clock_id != 0)
        TFAIL("notif->clock_id != 0");

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(RTC0018, VIRTIO_PCI_DEVICE_RTC, test_rtc_alarm_fire,
              "alarm SET then NOTIF_ALARM delivered on alarmq",
              VIRTIO_SPEC_V1_4, "5.23.6",
              0, 2);
