/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0022: rtc_read_invalid_clock_id
 *
 * Submit a READ_TIME request with clock_id set to 0xFFFF. Spec
 * draft RTC.5 says the device must validate clock_id and return
 * an error for unknown clocks without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rtc_bad_clock(struct virtio_dev *dev,
                                        struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 4096);

    struct rtc_req_read *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_READ;
    req->clock_id = 0xFFFF;

    struct rtc_resp_read *resp = (void *)(buf + 256);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req),
                       (uint32_t)sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp),
                       (uint32_t)sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RTC0022, VIRTIO_PCI_DEVICE_RTC, test_rtc_bad_clock,
              "READ_TIME with invalid clock_id 0xFFFF",
              VIRTIO_SPEC_V1_4, "5.23.6");
