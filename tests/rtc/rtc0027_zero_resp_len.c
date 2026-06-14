/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0027: RTC READ_TIME with zero response length.
 *
 * Spec RTC.5: Submit a READ_TIME request where the writable
 * response descriptor has length 0. The device must not write
 * past the descriptor boundary.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_RTC_REQ_READ_TIME 0x0001

static test_result_t test_rtc_zero_resp_len(struct virtio_dev *dev,
                                            struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 4096);

    struct rtc_req_read *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_READ_TIME;
    req->clock_id = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req),
                       (uint32_t)sizeof(*req), VRING_DESC_F_NEXT, 1);
    /* Zero length response */
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(buf) + 256, 0,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RTC0027, VIRTIO_PCI_DEVICE_RTC, test_rtc_zero_resp_len,
              "READ_TIME with zero length response descriptor",
              VIRTIO_SPEC_V1_4, "RTC.5");
