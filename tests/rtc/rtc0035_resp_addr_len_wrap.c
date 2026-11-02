/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0035: RTC read response writable addr plus len wraps 2^64.
 *
 * Siblings RTC0013 and RTC0023 keep the response end address below 2^64.
 * Here the response descriptor base sits near the top of the address
 * space and the length makes addr plus len wrap to a low value, so a
 * device that computes the end with a plain addition gets a small
 * wrapped result and a naive bounds check passes. The device must not
 * access memory outside the guest mapping or crash the VMM. Completing,
 * silently rejecting, or wedging the queue are all acceptable.
 *
 * Spec 5.23.6.
 */
#include "lib/virtio_test.h"

static test_result_t test_rtc_resp_addr_len_wrap(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    struct rtc_req_read *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_READ;

    vv_desc(vr, 0, buf, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, 0xFFFFFFFFFFFFF000ULL, 0x2000,
                       VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(RTC0035, VIRTIO_PCI_DEVICE_RTC, test_rtc_resp_addr_len_wrap,
              "RTC read response writable addr plus len wraps 64 bits",
              VIRTIO_SPEC_V1_4, "5.23.6");
