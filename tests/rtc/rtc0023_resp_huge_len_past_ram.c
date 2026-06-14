/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0023: RTC read response writable len past end of guest RAM.
 *
 * Same shape as RNG0004: submit a READ whose response descriptor
 * base lives in valid guest RAM but whose length crosses the end
 * of all System RAM. Device must not access memory outside the
 * guest's mapping or crash the VMM.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define VIRTIO_RTC_REQ_READ 0x0001

static test_result_t test_rtc_resp_huge_len_past_ram(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    uint64_t ram_top = vv_parse_ram_top();
    if (ram_top == 0)
        return TEST_SKIP;

    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 256);
    struct rtc_req_read *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_READ;
    uint64_t base = vv_virt_to_phys(buf);
    uint64_t resp_phys = base + 64;
    if (resp_phys >= ram_top)
        return TEST_SKIP;

    uint64_t overshoot = (ram_top - resp_phys) + (1ULL << 30);
    if (overshoot > 0xFFFFFFFFULL)
        overshoot = 0xFFFFFFFFULL;
    uint32_t len = (uint32_t)overshoot;

    vring_raw_set_desc(vr, 0, base, sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, resp_phys, len,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RTC0023, VIRTIO_PCI_DEVICE_RTC, test_rtc_resp_huge_len_past_ram,
              "RTC read response writable len crosses end of RAM",
              VIRTIO_SPEC_V1_4, "RTC.5");
