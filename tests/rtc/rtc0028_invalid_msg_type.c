/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0028: RTC READ_TIME with invalid msg_type.
 *
 * Spec RTC.5: Submit a request with msg_type=0xFFFF which is not
 * a valid RTC operation. The device must return an error without
 * crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct rtc_req_read {
    uint16_t msg_type;
    uint8_t  reserved[6];
    uint16_t clock_id;
    uint8_t  reserved2[6];
};

struct rtc_resp_read {
    uint8_t  status;
    uint8_t  reserved[7];
    uint64_t time_ns;
};

static test_result_t test_rtc_invalid_msg_type(struct virtio_dev *dev,
                                               struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 4096);

    struct rtc_req_read *req = (void *)buf;
    req->msg_type = 0xFFFF;
    req->clock_id = 0;

    struct rtc_resp_read *resp = (void *)(buf + 256);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req),
                       (uint32_t)sizeof(*req), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp),
                       (uint32_t)sizeof(*resp), VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RTC0028, VIRTIO_PCI_DEVICE_RTC, test_rtc_invalid_msg_type,
              "Request with invalid msg_type 0xFFFF",
              VIRTIO_SPEC_V1_4, "RTC.5");
