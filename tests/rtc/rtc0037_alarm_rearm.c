/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0037: re-arm an alarm with a second SET_ALARM.
 *
 * Spec 5.23.6: the driver may update an armed alarm by sending a
 * fresh SET_ALARM for the same clock. The second request replaces
 * the first and the device must accept both. This test sends two
 * SET_ALARM requests with FLAG_ALARM_ENABLED and different future
 * times and checks that each is acknowledged with status OK.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <stdint.h>

static test_result_t test_rtc_alarm_rearm(struct virtio_dev *dev,
                                          struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1u << VIRTIO_RTC_F_ALARM)))
        return TEST_SKIP;

    struct rtc_req_set_alarm_nested *req = vv_alloc_pages(1);
    struct rtc_resp_head *resp = vv_alloc_pages(1);

    for (int round = 0; round < 2; round++) {
        memset(req, 0, sizeof(*req));
        memset(resp, 0xFF, sizeof(*resp));
        req->head.msg_type = VIRTIO_RTC_REQ_SET_ALARM;
        req->clock_id = 0;
        req->flags = VIRTIO_RTC_FLAG_ALARM_ENABLED;
        req->alarm_time = (uint64_t)1 << (60 + round);

        vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                           VRING_DESC_F_NEXT, 1);
        vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, round, 0);
        vring_raw_set_avail_idx(vr, round + 1);

        test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
        if (r != TEST_PASS)
            return r;
        if (resp->status == VIRTIO_RTC_S_ENODEV)
            return TEST_SKIP;
        if (resp->status != VIRTIO_RTC_S_OK)
            TFAIL("re-arm round %d status %u", round, resp->status);
    }

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(RTC0037, VIRTIO_PCI_DEVICE_RTC, test_rtc_alarm_rearm,
              "Re-arm an alarm with a second SET_ALARM",
              VIRTIO_SPEC_V1_4, "5.23.6",
              (1ULL << VIRTIO_RTC_F_ALARM), 0);
