/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0005: RTC CROSS_CAP probe.
 *
 * VIRTIO_RTC_REQ_CROSS_CAP for clock 0 + x86 TSC. Device sets
 * VIRTIO_RTC_FLAG_CROSS_CAP iff cross-timestamping is supported.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <stdint.h>

#define VIRTIO_RTC_REQ_CROSS_CAP 0x1002
#define VIRTIO_RTC_COUNTER_X86_TSC 1

struct rtc_req_cross_cap {
    uint16_t msg_type; uint8_t r0[6];
    uint16_t clock_id; uint8_t hw_counter; uint8_t r1[5];
};
struct rtc_resp_cross_cap {
    uint8_t status; uint8_t r0[7];
    uint8_t flags; uint8_t r1[7];
};

static test_result_t test_rtc_cross_cap(struct virtio_dev *dev,
                                        struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 256);
    struct rtc_req_cross_cap *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_CROSS_CAP;
    req->hw_counter = VIRTIO_RTC_COUNTER_X86_TSC;
    uint64_t base = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, base, sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, base + 64,
                       sizeof(struct rtc_resp_cross_cap),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RTC0005, VIRTIO_PCI_DEVICE_RTC, test_rtc_cross_cap,
              "RTC CROSS_CAP probe",
              VIRTIO_SPEC_V1_4, "RTC.5");
