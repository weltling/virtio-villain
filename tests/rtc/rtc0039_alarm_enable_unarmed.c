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
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <stdint.h>

static test_result_t test_rtc_alarm_enable_unarmed(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1u << VIRTIO_RTC_F_ALARM)))
        return TEST_SKIP;

    struct rtc_req_set_alarm_nested *req = vv_alloc_pages(1);
    struct rtc_resp_head *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    memset(resp, 0xFF, sizeof(*resp));
    req->head.msg_type = VIRTIO_RTC_REQ_SET_ALARM_ENABLED;
    req->clock_id = 0;
    req->flags = VIRTIO_RTC_FLAG_ALARM_ENABLED;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

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
