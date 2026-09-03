/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0038: READ_ALARM returns a deterministic response.
 *
 * Spec 5.23.6: with VIRTIO_RTC_F_ALARM negotiated the driver may
 * query the current alarm for a clock with a READ_ALARM request.
 * The device must return a status byte and leave a well formed
 * response. This test issues READ_ALARM for clock 0 and checks
 * that the device fills the response status field and stays
 * alive, without asserting a particular alarm value.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <stdint.h>

/* READ_ALARM request. Header followed by the clock id, matching
 * the layout used by the other clock selecting requests. */
struct rtc_req_read_alarm {
    uint16_t msg_type;
    uint8_t  reserved[6];
    uint16_t clock_id;
    uint8_t  reserved2[6];
} __attribute__((packed));

static test_result_t test_rtc_read_alarm(struct virtio_dev *dev,
                                         struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1u << VIRTIO_RTC_F_ALARM)))
        return TEST_SKIP;

    struct rtc_req_read_alarm *req = vv_alloc_pages(1);
    uint8_t *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    memset(resp, 0xFF, 64);
    req->msg_type = VIRTIO_RTC_REQ_READ_ALARM;
    req->clock_id = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), 64,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    /* status byte is at offset 0 in every RTC response. A value of
     * ENODEV means the clock is absent, which is a valid answer. */
    if (resp[0] == VIRTIO_RTC_S_ENODEV)
        return TEST_SKIP;
    if (resp[0] == 0xFF)
        TFAIL("device left response status untouched");

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(RTC0038, VIRTIO_PCI_DEVICE_RTC, test_rtc_read_alarm,
              "READ_ALARM returns a deterministic response",
              VIRTIO_SPEC_V1_4, "5.23.6",
              (1ULL << VIRTIO_RTC_F_ALARM), 0);
