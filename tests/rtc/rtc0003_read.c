/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0003: RTC READ clock 0.
 *
 * VIRTIO_RTC_REQ_READ for clock id 0. The device fills
 * clock_reading in nanoseconds and sets status=OK.
 */
#include "lib/virtio_test.h"

static test_result_t test_rtc_read(struct virtio_dev *dev,
                                   struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    struct rtc_req_read *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_READ;
    uint64_t base = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, base, sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, base + 64, sizeof(struct rtc_resp_read),
                       VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(RTC0003, VIRTIO_PCI_DEVICE_RTC, test_rtc_read,
              "RTC READ clock 0",
              VIRTIO_SPEC_V1_4, "5.23.6");
