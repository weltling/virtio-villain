/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0021: rtc_read_time_twice
 *
 * Read current time from the requestq twice in succession and
 * verify the second reading is not earlier than the first. Spec
 * 5.23.6 says time must be monotonically nondecreasing.
 * The device must return consistent timestamps.
 */
#include "lib/virtio_test.h"

static test_result_t test_rtc_read_twice(struct virtio_dev *dev,
                                         struct vring *vr)
{
    uint8_t *buf1 = vv_alloc_pages(1);
    uint8_t *buf2 = vv_alloc_pages(1);

    struct rtc_req_read *req1 = (void *)buf1;
    req1->msg_type = VIRTIO_RTC_REQ_READ;
    req1->clock_id = 0;
    struct rtc_resp_read *resp1 = (void *)(buf1 + 256);

    size_t req_len = sizeof(*req1);
    size_t resp_len = sizeof(*resp1);

    vv_desc(vr, 0, req1, (uint32_t)req_len, VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp1, (uint32_t)resp_len, VRING_DESC_F_WRITE, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r != TEST_PASS)
        return r;

    uint64_t t1 = resp1->time_ns;

    /* Second read */
    struct rtc_req_read *req2 = (void *)buf2;
    req2->msg_type = VIRTIO_RTC_REQ_READ;
    req2->clock_id = 0;
    struct rtc_resp_read *resp2 = (void *)(buf2 + 256);

    vv_desc(vr, 2, req2, (uint32_t)req_len, VRING_DESC_F_NEXT, 3);
    vv_desc(vr, 3, resp2, (uint32_t)resp_len, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    uint64_t t2 = resp2->time_ns;
    if (t2 < t1)
        TFAIL("t2 < t1");
    return TEST_PASS;
}

REGISTER_TEST(RTC0021, VIRTIO_PCI_DEVICE_RTC, test_rtc_read_twice,
              "Two successive reads return nondecreasing time",
              VIRTIO_SPEC_V1_4, "5.23.6");
