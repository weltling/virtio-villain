/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0017: 32 sequential clock reads, monotonic and OK
 *
 * Spec RTC.5 says VIRTIO_RTC_REQ_READ returns the current clock
 * value in nanoseconds. The clock must be monotonic, so a series
 * of reads in quick succession must produce non decreasing values.
 * Submit 32 reads, each with its own request and response buffer,
 * and verify both ordering and OK status on every reply.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static int do_one(struct virtio_dev *dev, struct vring *vr,
                  uint16_t base, uint16_t avail_slot, uint64_t *out)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 256);
    struct rtc_req_read *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_READ;
    uint64_t bphys = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, base, bphys, sizeof(*req),
                       VRING_DESC_F_NEXT, base + 1);
    vring_raw_set_desc(vr, base + 1, bphys + 64, sizeof(struct rtc_resp_read),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, avail_slot, base);
    vring_raw_set_avail_idx(vr, avail_slot + 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return -1;

    struct rtc_resp_read *resp = (void *)(buf + 64);
    if (resp->status != 0)
        return -1;
    *out = resp->time_ns;
    return 0;
}

static test_result_t test_rtc_monotonic(struct virtio_dev *dev,
                                        struct vring *vr)
{
    uint64_t prev = 0;
    int n = 32;
    if ((int)vr->size < n * 2 + 2)
        n = (vr->size - 2) / 2;
    if (n < 4)
        return TEST_SKIP;

    for (int i = 0; i < n; i++) {
        uint64_t cur = 0;
        if (do_one(dev, vr, i * 2, i, &cur) < 0)
            TFAIL("do_one(dev, vr, i * 2, i, &cur) < 0");
        if (cur < prev)
            TFAIL("cur < prev");
        prev = cur;
    }
    return TEST_PASS;
}

REGISTER_TEST(RTC0017, VIRTIO_PCI_DEVICE_RTC, test_rtc_monotonic,
              "32 sequential clock reads stay monotonic",
              VIRTIO_SPEC_V1_4, "5.23.6");
