/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0032: CLOCK_CAP with an out of range clock_id.
 *
 * v1.4 5.23.3: VIRTIO_RTC_F_CLOCK_CAP_CLOCK_ID extends the
 * CLOCK_CAP query to accept multiple clock identifiers. Submit
 * a query for a clearly invalid clock_id (0xFFFF). A device that
 * does not validate the clock_id and indexes into an internal
 * array can return garbage or crash. The device must reject the
 * id with an INVAL status.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

#define VIRTIO_RTC_F_CLOCK_CAP_CLOCK_ID 4
#define VIRTIO_RTC_REQ_CLOCK_CAP        0x1001

static test_result_t test_rtc_clock_cap_clock_id(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_RTC_F_CLOCK_CAP_CLOCK_ID)))
        return TEST_SKIP;

    struct rtc_req_clock_cap  *req  = vv_alloc_pages(1);
    struct rtc_resp_clock_cap *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    memset(resp, 0, sizeof(*resp));
    req->msg_type = VIRTIO_RTC_REQ_CLOCK_CAP;
    req->clock_id = 0xFFFF;
    resp->status  = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RTC0032, VIRTIO_PCI_DEVICE_RTC,
              test_rtc_clock_cap_clock_id,
              "CLOCK_CAP with out of range clock_id",
              VIRTIO_SPEC_V1_4, "5.23.3");
