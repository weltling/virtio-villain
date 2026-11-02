/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0013: RTC huge response length.
 *
 * Submit READ with a writable descriptor whose len is 1 GiB but
 * which is backed by a single page. The device must clamp writes
 * or refuse the request rather than overrun the page.
 */
#include "lib/virtio_test.h"

static test_result_t test_rtc_huge_resp(struct virtio_dev *dev,
                                        struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    struct rtc_req_read *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_READ;
    uint64_t resp_phys;
    vv_alloc_page_high(&resp_phys);

    vv_desc(vr, 0, buf, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, resp_phys, 1u << 30,
                       VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(RTC0013, VIRTIO_PCI_DEVICE_RTC, test_rtc_huge_resp,
              "RTC huge response length",
              VIRTIO_SPEC_V1_4, "5.23.6");
