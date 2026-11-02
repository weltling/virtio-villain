/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0034: alarm disable via SET_ALARM_ENABLED with flag cleared.
 *
 * Spec 5.23.6: the driver can disable a previously armed alarm
 * by sending SET_ALARM_ENABLED with the enabled flag cleared.
 * The device must accept the request and stop delivering
 * NOTIF_ALARM notifications for that clock.
 */
#include "lib/virtio_test.h"

static test_result_t test_rtc_alarm_disable(struct virtio_dev *dev,
                                            struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_RTC_F_ALARM))
        return TEST_SKIP;

    /* SET_ALARM_ENABLED with flag=0 (disable) */
    struct rtc_req_set_alarm_nested *req = vv_alloc_pages(1);
    struct rtc_resp_head *resp = vv_alloc_pages(1);

    req->head.msg_type = VIRTIO_RTC_REQ_SET_ALARM_ENABLED;
    req->clock_id = 0;
    req->flags = 0;  /* disable alarm */

    memset(resp, 0xFF, sizeof(*resp));

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);

    VV_TRY(vv_submit_and_wait(dev, vr, 0));

    if (resp->status == VIRTIO_RTC_S_ENODEV)
        return TEST_SKIP;
    if (resp->status != VIRTIO_RTC_S_OK)
        TFAIL("status %u, expected OK", resp->status);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(RTC0034, VIRTIO_PCI_DEVICE_RTC, test_rtc_alarm_disable,
              "Disable alarm via SET_ALARM_ENABLED with flag cleared",
              VIRTIO_SPEC_V1_4, "5.23.6",
              (1ULL << VIRTIO_RTC_F_ALARM), 0);
