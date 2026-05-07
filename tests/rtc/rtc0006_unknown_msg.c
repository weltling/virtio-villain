/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0006: RTC unknown msg_type.
 *
 * Send a header with msg_type = 0xDEAD. Per spec the device must
 * set status=VIRTIO_RTC_S_EOPNOTSUPP for unknown message types.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <stdint.h>

struct rtc_req_head { uint16_t msg_type; uint8_t reserved[6]; };

static test_result_t test_rtc_unknown_msg(struct virtio_dev *dev,
                                          struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 256);
    struct rtc_req_head *req = (void *)buf;
    req->msg_type = 0xDEAD;
    uint64_t base = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, base, sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, base + 64, 64, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RTC0006, VIRTIO_PCI_DEVICE_RTC, test_rtc_unknown_msg,
              "RTC unknown msg_type",
              VIRTIO_SPEC_V1_4, "RTC.5");
