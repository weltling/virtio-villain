/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0022: rtc_read_invalid_clock_id
 *
 * Submit a READ_TIME request with clock_id set to 0xFFFF. Spec
 * 5.23.6 says the device must validate clock_id and return
 * an error for unknown clocks without crashing.
 */
#include "lib/virtio_test.h"

static test_result_t test_rtc_bad_clock(struct virtio_dev *dev,
                                        struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    struct rtc_req_read *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_READ;
    req->clock_id = 0xFFFF;

    struct rtc_resp_read *resp = (void *)(buf + 256);

    vv_desc(vr, 0, req, (uint32_t)sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, (uint32_t)sizeof(*resp), VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(RTC0022, VIRTIO_PCI_DEVICE_RTC, test_rtc_bad_clock,
              "READ_TIME with invalid clock_id 0xFFFF",
              VIRTIO_SPEC_V1_4, "5.23.6");
