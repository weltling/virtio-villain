/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0031: CLOCK_CAP response split across two writable descriptors.
 *
 * Spec 5.23.6: the device must honour the descriptor chain
 * geometry when writing a response. The 16-byte CLOCK_CAP response
 * is offered as two writable descriptors (status half and capability
 * half). A device that assumes a single contiguous response buffer
 * may write past the first descriptor or corrupt guest memory.
 */
#include "lib/virtio_test.h"

static test_result_t test_rtc_clock_cap_split(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    struct rtc_req_clock_cap_legacy *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_CLOCK_CAP;
    req->clock_id = 0;

    /* Sentinel byte right after the second response chunk so an
     * overrun past the offered region is visible. */
    uint8_t *resp = buf + 256;
    const size_t half = 8;
    const size_t rest = sizeof(struct rtc_resp_clock_cap_legacy) - half;
    resp[half + rest] = 0xAB;

    vv_desc(vr, 0, req, (uint32_t)sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, (uint32_t)half, VRING_DESC_F_WRITE | VRING_DESC_F_NEXT,
            2);
    vv_desc(vr, 2, resp + half, (uint32_t)rest, VRING_DESC_F_WRITE, 0);
    VV_TRY(vv_submit_and_wait(dev, vr, 0));

    if (resp[half + rest] != 0xAB)
        TFAIL("device wrote past the offered response region");
    return TEST_PASS;
}

REGISTER_TEST(RTC0031, VIRTIO_PCI_DEVICE_RTC, test_rtc_clock_cap_split,
              "CLOCK_CAP response split across two descriptors",
              VIRTIO_SPEC_V1_4, "5.23.6");
