/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0008: RTC invalid hw_counter.
 *
 * VIRTIO_RTC_REQ_READ_CROSS with hw_counter=0x10 (neither
 * standard nor in 0xF0..0xFE experimental range). Per spec the
 * device sets status=VIRTIO_RTC_S_EOPNOTSUPP for unknown counter
 * ids.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <stdint.h>

static test_result_t test_rtc_invalid_hw(struct virtio_dev *dev,
                                         struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 256);
    struct rtc_req_read_cross *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_READ_CROSS;
    req->hw_counter = 0x10;
    uint64_t base = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, base, sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, base + 64, 24, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RTC0008, VIRTIO_PCI_DEVICE_RTC, test_rtc_invalid_hw,
              "RTC invalid hw_counter",
              VIRTIO_SPEC_V1_4, "RTC.5");
