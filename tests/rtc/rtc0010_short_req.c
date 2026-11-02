/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0010: RTC truncated request header.
 *
 * Provide a readable request descriptor of 1 byte, smaller than
 * struct virtio_rtc_req_head. Per spec the device MUST set
 * status=VIRTIO_RTC_S_EINVAL if the request does not fit into
 * the device-readable buffer.
 */
#include "lib/virtio_test.h"

static test_result_t test_rtc_short_req(struct virtio_dev *dev,
                                        struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t base = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, base, 1, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, base + 64, 32, VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(RTC0010, VIRTIO_PCI_DEVICE_RTC, test_rtc_short_req,
              "RTC truncated request header",
              VIRTIO_SPEC_V1_4, "5.23.6");
