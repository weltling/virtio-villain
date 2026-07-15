/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0034: alarm disable via SET_ALARM_ENABLED with flag cleared.
 *
 * Spec RTC.5: the driver can disable a previously armed alarm
 * by sending SET_ALARM_ENABLED with the enabled flag cleared.
 * The device must accept the request and stop delivering
 * NOTIF_ALARM notifications for that clock.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rtc_alarm_disable(struct virtio_dev *dev,
                                            struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1u << VIRTIO_RTC_F_ALARM)))
        return TEST_SKIP;

    /* SET_ALARM_ENABLED with flag=0 (disable) */
    struct rtc_req_set_alarm_nested *req = vv_alloc_pages(1);
    struct rtc_resp_head *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    req->head.msg_type = VIRTIO_RTC_REQ_SET_ALARM_ENABLED;
    req->clock_id = 0;
    req->flags = 0;  /* disable alarm */

    memset(resp, 0xFF, sizeof(*resp));

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
    if (resp->status != VIRTIO_RTC_S_OK)
        TFAIL("status %u, expected OK", resp->status);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(RTC0034, VIRTIO_PCI_DEVICE_RTC, test_rtc_alarm_disable,
              "Disable alarm via SET_ALARM_ENABLED with flag cleared",
              VIRTIO_SPEC_V1_4, "RTC.5",
              (1ULL << VIRTIO_RTC_F_ALARM), 0);
