/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0001: RTC basic CFG request.
 *
 * Submit a VIRTIO_RTC_REQ_CFG message: a readable header
 * descriptor chained to a writable response descriptor on the
 * requestq (queue 0). Per spec the device fills num_clocks and
 * sets status=OK.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <stdint.h>

#define VIRTIO_RTC_REQ_CFG 0x1000

static test_result_t test_rtc_basic(struct virtio_dev *dev,
                                    struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 256);
    struct rtc_req_head *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_CFG;
    uint64_t base = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, base, sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, base + 64, sizeof(struct rtc_resp_cfg),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RTC0001, VIRTIO_PCI_DEVICE_RTC, test_rtc_basic,
              "RTC basic CFG request",
              VIRTIO_SPEC_V1_4, "RTC.5");
