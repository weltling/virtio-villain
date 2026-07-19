/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0031: CLOCK_CAP response split across two writable descriptors.
 *
 * Spec draft RTC.5: the device must honour the descriptor chain
 * geometry when writing a response. The 16-byte CLOCK_CAP response
 * is offered as two writable descriptors (status half and capability
 * half). A device that assumes a single contiguous response buffer
 * may write past the first descriptor or corrupt guest memory.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <stdint.h>

static test_result_t test_rtc_clock_cap_split(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 4096);

    struct rtc_req_clock_cap_legacy *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_CLOCK_CAP;
    req->clock_id = 0;

    /* Sentinel byte right after the second response chunk so an
     * overrun past the offered region is visible. */
    uint8_t *resp = buf + 256;
    const size_t half = 8;
    const size_t rest = sizeof(struct rtc_resp_clock_cap_legacy) - half;
    resp[half + rest] = 0xAB;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req),
                       (uint32_t)sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp),
                       (uint32_t)half,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(resp + half),
                       (uint32_t)rest,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    if (resp[half + rest] != 0xAB)
        TFAIL("device wrote past the offered response region");
    return TEST_PASS;
}

REGISTER_TEST(RTC0031, VIRTIO_PCI_DEVICE_RTC, test_rtc_clock_cap_split,
              "CLOCK_CAP response split across two descriptors",
              VIRTIO_SPEC_V1_4, "5.23.6");
