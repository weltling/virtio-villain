/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0032: CLOCK_CAP with an out-of-range clock_id.
 *
 * Spec draft RTC.5: the device must validate clock_id and report an
 * error status for clocks it does not expose, rather than indexing a
 * clock table out of bounds. This probes the capability path (rather
 * than the time read path covered by RTC0022) with clock_id 0xFFFF.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <stdint.h>

static test_result_t test_rtc_clock_cap_bad_id(struct virtio_dev *dev,
                                               struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 4096);

    struct rtc_req_clock_cap_legacy *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_CLOCK_CAP;
    req->clock_id = 0xFFFF;

    struct rtc_resp_clock_cap_legacy *resp = (void *)(buf + 256);
    resp->status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req),
                       (uint32_t)sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp),
                       (uint32_t)sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    if (resp->status == VIRTIO_RTC_S_OK)
        TFAIL("device accepted CLOCK_CAP for clock_id 0xFFFF");
    return TEST_PASS;
}

REGISTER_TEST(RTC0032, VIRTIO_PCI_DEVICE_RTC, test_rtc_clock_cap_bad_id,
              "CLOCK_CAP with out-of-range clock_id 0xFFFF",
              VIRTIO_SPEC_V1_4, "5.23.6");
