/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0039: SET_ALARM_ENABLED on a clock that was never armed.
 *
 * Spec 5.23.6: SET_ALARM_ENABLED toggles delivery for an alarm
 * that has been set with SET_ALARM. Enabling delivery for a clock
 * with no armed alarm has no valid alarm to enable and the device
 * must respond deterministically rather than crash or stay silent.
 * This test sends SET_ALARM_ENABLED with the enabled flag set and
 * no prior SET_ALARM, then checks that the device answers.
 */
#include "lib/virtio_test.h"

static test_result_t test_rtc_alarm_enable_unarmed(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_RTC_F_ALARM))
        return TEST_SKIP;

    struct rtc_req_set_alarm_nested *req = vv_alloc_pages(1);
    struct rtc_resp_head *resp = vv_alloc_pages(1);
    memset(resp, 0xFF, sizeof(*resp));
    req->head.msg_type = VIRTIO_RTC_REQ_SET_ALARM_ENABLED;
    req->clock_id = 0;
    req->flags = VIRTIO_RTC_FLAG_ALARM_ENABLED;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    VV_TRY(vv_submit_and_wait(dev, vr, 0));

    if (resp->status == VIRTIO_RTC_S_ENODEV)
        return TEST_SKIP;
    /* Either OK or an error status is acceptable; the device must
     * fill the response and remain alive. */
    if (resp->status == 0xFF)
        TFAIL("device left response status untouched");

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(RTC0039, VIRTIO_PCI_DEVICE_RTC,
              test_rtc_alarm_enable_unarmed,
              "SET_ALARM_ENABLED on a clock that was never armed",
              VIRTIO_SPEC_V1_4, "5.23.6",
              (1ULL << VIRTIO_RTC_F_ALARM), 0);
