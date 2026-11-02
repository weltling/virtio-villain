/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0027: RTC READ_TIME with zero response length.
 *
 * Spec 5.23.6: Submit a READ_TIME request where the writable
 * response descriptor has length 0. The device must not write
 * past the descriptor boundary.
 */
#include "lib/virtio_test.h"

static test_result_t test_rtc_zero_resp_len(struct virtio_dev *dev,
                                            struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    struct rtc_req_read *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_READ;
    req->clock_id = 0;

    vv_desc(vr, 0, req, (uint32_t)sizeof(*req), VRING_DESC_F_NEXT, 1);
    /* Zero length response */
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(buf) + 256, 0,
                       VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(RTC0027, VIRTIO_PCI_DEVICE_RTC, test_rtc_zero_resp_len,
              "READ_TIME with zero length response descriptor",
              VIRTIO_SPEC_V1_4, "5.23.6");
