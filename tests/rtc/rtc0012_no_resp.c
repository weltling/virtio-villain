/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0012: RTC missing writable response.
 *
 * Submit READ with only a readable header descriptor and no
 * writable response buffer. Per spec the driver MUST allocate
 * enough writable space; a compliant device must reject the
 * malformed chain rather than scribble somewhere.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <stdint.h>

#define VIRTIO_RTC_REQ_READ 0x0001

struct rtc_req_read {
    uint16_t msg_type; uint8_t r0[6];
    uint16_t clock_id; uint8_t r1[6];
};

static test_result_t test_rtc_no_resp(struct virtio_dev *dev,
                                      struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 256);
    struct rtc_req_read *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_READ;
    uint64_t base = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, base, sizeof(*req), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RTC0012, VIRTIO_PCI_DEVICE_RTC, test_rtc_no_resp,
              "RTC missing writable response",
              VIRTIO_SPEC_V1_4, "RTC.5");
