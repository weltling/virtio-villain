/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0009: RTC truncated response buffer.
 *
 * Submit READ with a writable response buffer of only 1 byte,
 * smaller than struct virtio_rtc_resp_read. Per spec the device
 * MUST set status=VIRTIO_RTC_S_EINVAL when the response does not
 * fit, unless even status itself does not fit.
 */
#include "lib/virtio_test.h"

static test_result_t test_rtc_short_resp(struct virtio_dev *dev,
                                         struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    struct rtc_req_read *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_READ;
    uint64_t base = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, base, sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, base + 64, 1, VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(RTC0009, VIRTIO_PCI_DEVICE_RTC, test_rtc_short_resp,
              "RTC truncated response buffer",
              VIRTIO_SPEC_V1_4, "5.23.6");
