/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0028: RTC READ_TIME with invalid msg_type.
 *
 * Spec 5.23.6: Submit a request with msg_type=0xFFFF which is not
 * a valid RTC operation. The device must return an error without
 * crashing.
 */
#include "lib/virtio_test.h"

static test_result_t test_rtc_invalid_msg_type(struct virtio_dev *dev,
                                               struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    struct rtc_req_read *req = (void *)buf;
    req->msg_type = 0xFFFF;
    req->clock_id = 0;

    struct rtc_resp_read *resp = (void *)(buf + 256);

    vv_desc(vr, 0, req, (uint32_t)sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, (uint32_t)sizeof(*resp), VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(RTC0028, VIRTIO_PCI_DEVICE_RTC, test_rtc_invalid_msg_type,
              "Request with invalid msg_type 0xFFFF",
              VIRTIO_SPEC_V1_4, "5.23.6");
