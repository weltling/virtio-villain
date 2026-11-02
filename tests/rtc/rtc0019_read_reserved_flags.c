/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0019: rtc_read_reserved_flags
 *
 * Submit a READ request whose reserved bytes in the message
 * header are non zero. Spec 5.23.6 marks them reserved,
 * requiring the device to reject the request without reading
 * the clock. The device must stay alive.
 */
#include "lib/virtio_test.h"

static test_result_t test_rtc_read_reserved(struct virtio_dev *dev,
                                            struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    struct rtc_req_read *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_READ;
    /* Stuff reserved with all bits set */
    memset(req->reserved, 0xFF, sizeof(req->reserved));
    req->clock_id = 0;

    uint64_t base = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, base, sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, base + 64, sizeof(struct rtc_resp_read),
                       VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(RTC0019, VIRTIO_PCI_DEVICE_RTC, test_rtc_read_reserved,
              "READ with reserved bytes set in the header",
              VIRTIO_SPEC_V1_4, "5.23.6");
