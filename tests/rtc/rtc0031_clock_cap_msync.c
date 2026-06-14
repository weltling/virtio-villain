/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0031: CLOCK_CAP_MSYNC capability query.
 *
 * v1.4 5.23.3 defines VIRTIO_RTC_F_CLOCK_CAP_MSYNC (bit ~3) which
 * extends VIRTIO_RTC_REQ_CLOCK_CAP to report msync (cross
 * timestamping) capability flags. Issue a CLOCK_CAP query with
 * clock_id 0. A device that does not implement the feature
 * should stay silent rather than fill garbage into the response.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

#define VIRTIO_RTC_F_CLOCK_CAP_MSYNC 3
#define VIRTIO_RTC_REQ_CLOCK_CAP     0x1001

static test_result_t test_rtc_clock_cap_msync(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_RTC_F_CLOCK_CAP_MSYNC)))
        return TEST_SKIP;

    struct rtc_req_clock_cap  *req  = vv_alloc_pages(1);
    struct rtc_resp_clock_cap *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    memset(resp, 0, sizeof(*resp));
    req->msg_type = VIRTIO_RTC_REQ_CLOCK_CAP;
    req->clock_id = 0;
    resp->status  = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RTC0031, VIRTIO_PCI_DEVICE_RTC,
              test_rtc_clock_cap_msync,
              "CLOCK_CAP query advertising msync capability",
              VIRTIO_SPEC_V1_4, "5.23.3");
