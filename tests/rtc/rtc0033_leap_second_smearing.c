/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0033: READ with LEAP_SECOND_SMEARING feature negotiated.
 *
 * v1.4 5.23.3: VIRTIO_RTC_F_LEAP_SECOND_SMEARING signals the
 * device smears leap seconds across a window rather than
 * applying the jump abruptly. The driver visible API does not
 * change; the test verifies a plain READ still completes when
 * the feature is advertised.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

#define VIRTIO_RTC_F_LEAP_SECOND_SMEARING 5
#define VIRTIO_RTC_REQ_READ               0x0001

static test_result_t test_rtc_leap_second_smearing(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_RTC_F_LEAP_SECOND_SMEARING)))
        return TEST_SKIP;

    struct rtc_req_read_packed  *req  = vv_alloc_pages(1);
    struct rtc_resp_read_packed *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    memset(resp, 0, sizeof(*resp));
    req->msg_type = VIRTIO_RTC_REQ_READ;
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

REGISTER_TEST(RTC0033, VIRTIO_PCI_DEVICE_RTC,
              test_rtc_leap_second_smearing,
              "READ when leap second smearing feature is offered",
              VIRTIO_SPEC_V1_4, "5.23.3");
