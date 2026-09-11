/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0041: RTC CFG response reports at least one clock.
 *
 * RTC0001 submits VIRTIO_RTC_REQ_CFG and only checks the device
 * responds. Here the response is validated: the status must be
 * VIRTIO_RTC_S_OK and num_clocks must be at least one, since a device
 * must expose at least the UTC clock for a read to be possible.
 *
 * Spec 5.23.6.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <stdint.h>

static test_result_t test_rtc_cfg_num_clocks(struct virtio_dev *dev,
                                             struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 256);
    struct rtc_req_head *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_CFG;

    struct rtc_resp_cfg *resp = (struct rtc_resp_cfg *)(buf + 64);
    resp->status = 0xFF;

    uint64_t base = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, base, sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, base + 64, sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    if (resp->status != VIRTIO_RTC_S_OK)
        TFAIL("CFG status %u, expected OK", resp->status);
    if (resp->num_clocks < 1)
        TFAIL("CFG reports num_clocks 0");

    return TEST_PASS;
}

REGISTER_TEST(RTC0041, VIRTIO_PCI_DEVICE_RTC, test_rtc_cfg_num_clocks,
              "RTC CFG response reports at least one clock",
              VIRTIO_SPEC_V1_4, "5.23.6");
