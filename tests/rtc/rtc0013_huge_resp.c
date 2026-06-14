/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0013: RTC huge response length.
 *
 * Submit READ with a writable descriptor whose len is 1 GiB but
 * which is backed by a single page. The device must clamp writes
 * or refuse the request rather than overrun the page.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <stdint.h>

#define VIRTIO_RTC_REQ_READ 0x0001

static test_result_t test_rtc_huge_resp(struct virtio_dev *dev,
                                        struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 256);
    struct rtc_req_read *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_READ;
    uint64_t base = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, base, sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, base + 64, 1u << 30,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RTC0013, VIRTIO_PCI_DEVICE_RTC, test_rtc_huge_resp,
              "RTC huge response length",
              VIRTIO_SPEC_V1_4, "RTC.5");
