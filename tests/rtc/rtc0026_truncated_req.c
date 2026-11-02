/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0026: RTC READ_TIME with truncated request.
 *
 * Spec 5.23.6: Submit a READ_TIME request descriptor whose length
 * is smaller than sizeof(rtc_req_read). The device must detect the
 * short request and not read beyond the descriptor.
 */
#include "lib/virtio_test.h"

static test_result_t test_rtc_truncated_req(struct virtio_dev *dev,
                                            struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    struct rtc_req_read *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_READ;
    req->clock_id = 0;

    struct rtc_resp_read *resp = (void *)(buf + 256);

    /* Request descriptor only 2 bytes (just msg_type) */
    vv_desc(vr, 0, req, 2, VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, (uint32_t)sizeof(*resp), VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(RTC0026, VIRTIO_PCI_DEVICE_RTC, test_rtc_truncated_req,
              "READ_TIME with truncated request descriptor",
              VIRTIO_SPEC_V1_4, "5.23.6");
