/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0033: CLOCK_CAP reserved fields must be zeroed.
 *
 * Spec 5.23.6: reserved fields in a response MUST be set to
 * zero by the device. A device that copies a stack or heap structure
 * verbatim can leak uninitialised host memory through the trailing
 * reserved bytes of the CLOCK_CAP response. This sends a valid
 * CLOCK_CAP for clock_id 0 and inspects the reserved padding.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <stdint.h>

static test_result_t test_rtc_clock_cap_smearing(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 4096);

    struct rtc_req_clock_cap_legacy *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_CLOCK_CAP;
    req->clock_id = 0;

    struct rtc_resp_clock_cap_legacy *resp = (void *)(buf + 256);
    /* Poison the response so leftover bytes are distinguishable from
     * a device that correctly zeroes the reserved fields. */
    memset(resp, 0xCC, sizeof(*resp));

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req),
                       (uint32_t)sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp),
                       (uint32_t)sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    /* A device that rejects clock_id 0 tells us nothing about the
     * reserved fields, so only inspect them on success. */
    if (resp->status != VIRTIO_RTC_S_OK)
        return TEST_PASS;

    for (size_t i = 0; i < sizeof(resp->r1); i++) {
        if (resp->r1[i] != 0)
            TFAIL("device left CLOCK_CAP reserved bytes nonzero");
    }
    for (size_t i = 0; i < sizeof(resp->r0); i++) {
        if (resp->r0[i] != 0)
            TFAIL("device left CLOCK_CAP reserved bytes nonzero");
    }
    return TEST_PASS;
}

REGISTER_TEST(RTC0033, VIRTIO_PCI_DEVICE_RTC, test_rtc_clock_cap_smearing,
              "CLOCK_CAP reserved fields are zeroed",
              VIRTIO_SPEC_V1_4, "5.23.6");
