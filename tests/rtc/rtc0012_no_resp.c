/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0012: RTC missing writable response.
 *
 * Submit READ with only a readable header descriptor and no
 * writable response buffer. Per spec the driver MUST allocate
 * enough writable space; a compliant device must reject the
 * malformed chain rather than scribble somewhere.
 */
#include "lib/virtio_test.h"

static test_result_t test_rtc_no_resp(struct virtio_dev *dev,
                                      struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    struct rtc_req_read *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_READ;

    vv_desc(vr, 0, buf, sizeof(*req), 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(RTC0012, VIRTIO_PCI_DEVICE_RTC, test_rtc_no_resp,
              "RTC missing writable response",
              VIRTIO_SPEC_V1_4, "5.23.6");
