/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0021: rtc_read_time_twice
 *
 * Read current time from the requestq twice in succession and
 * verify the second reading is not earlier than the first. Spec
 * draft RTC.5 says time must be monotonically nondecreasing.
 * The device must return consistent timestamps.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_RTC_REQ_READ_TIME 0x0001

static test_result_t test_rtc_read_twice(struct virtio_dev *dev,
                                         struct vring *vr)
{
    uint8_t *buf1 = vv_alloc_pages(1);
    uint8_t *buf2 = vv_alloc_pages(1);
    memset(buf1, 0, 4096);
    memset(buf2, 0, 4096);

    struct rtc_req_read *req1 = (void *)buf1;
    req1->msg_type = VIRTIO_RTC_REQ_READ_TIME;
    req1->clock_id = 0;
    struct rtc_resp_read *resp1 = (void *)(buf1 + 256);

    size_t req_len = sizeof(*req1);
    size_t resp_len = sizeof(*resp1);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req1), (uint32_t)req_len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp1), (uint32_t)resp_len,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    uint64_t t1 = resp1->time_ns;

    /* Second read */
    struct rtc_req_read *req2 = (void *)buf2;
    req2->msg_type = VIRTIO_RTC_REQ_READ_TIME;
    req2->clock_id = 0;
    struct rtc_resp_read *resp2 = (void *)(buf2 + 256);

    vring_raw_set_desc(vr, 2, vv_virt_to_phys(req2), (uint32_t)req_len,
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(resp2), (uint32_t)resp_len,
                       VRING_DESC_F_WRITE, 0);
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
              VIRTIO_SPEC_V1_4, "RTC.5");
