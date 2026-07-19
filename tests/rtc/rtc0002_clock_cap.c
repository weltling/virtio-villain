/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0002: RTC CLOCK_CAP request.
 *
 * Probe capabilities of clock id 0. Spec: device fills type,
 * leap_second_smearing, flags, status=OK if the clock exists.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <stdint.h>

static test_result_t test_rtc_clock_cap(struct virtio_dev *dev,
                                        struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 256);
    struct rtc_req_clock_cap_legacy *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_CLOCK_CAP;
    req->clock_id = 0;
    uint64_t base = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, base, sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, base + 64, sizeof(struct rtc_resp_clock_cap_legacy),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RTC0002, VIRTIO_PCI_DEVICE_RTC, test_rtc_clock_cap,
              "RTC CLOCK_CAP request",
              VIRTIO_SPEC_V1_4, "5.23.6");
