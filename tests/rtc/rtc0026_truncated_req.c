/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0026: RTC READ_TIME with truncated request.
 *
 * Spec RTC.5: Submit a READ_TIME request descriptor whose length
 * is smaller than sizeof(rtc_req_read). The device must detect the
 * short request and not read beyond the descriptor.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rtc_truncated_req(struct virtio_dev *dev,
                                            struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 4096);

    struct rtc_req_read *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_READ;
    req->clock_id = 0;

    struct rtc_resp_read *resp = (void *)(buf + 256);

    /* Request descriptor only 2 bytes (just msg_type) */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), 2,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp),
                       (uint32_t)sizeof(*resp), VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RTC0026, VIRTIO_PCI_DEVICE_RTC, test_rtc_truncated_req,
              "READ_TIME with truncated request descriptor",
              VIRTIO_SPEC_V1_4, "RTC.5");
