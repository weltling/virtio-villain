/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0004: RTC READ_CROSS with x86 TSC.
 *
 * Cross-timestamp read for clock 0 against
 * VIRTIO_RTC_COUNTER_X86_TSC (id 1). Device returns
 * clock_reading + counter_cycles.
 */
#include "lib/virtio_test.h"

static test_result_t test_rtc_read_cross(struct virtio_dev *dev,
                                         struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    struct rtc_req_read_cross *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_READ_CROSS;
    req->hw_counter = VIRTIO_RTC_COUNTER_X86_TSC;
    uint64_t base = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, base, sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, base + 64,
                       sizeof(struct rtc_resp_read_cross),
                       VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(RTC0004, VIRTIO_PCI_DEVICE_RTC, test_rtc_read_cross,
              "RTC READ_CROSS x86 TSC",
              VIRTIO_SPEC_V1_4, "5.23.6");
