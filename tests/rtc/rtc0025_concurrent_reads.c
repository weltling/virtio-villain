/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0025: RTC concurrent read requests.
 *
 * Spec RTC.5: Submit two READ_TIME requests in the same avail
 * ring batch. The device must complete both without confusion.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rtc_concurrent_reads(struct virtio_dev *dev,
                                               struct vring *vr)
{
    uint8_t *p1 = vv_alloc_pages(1);
    uint8_t *p2 = vv_alloc_pages(1);
    memset(p1, 0, 4096);
    memset(p2, 0, 4096);

    struct rtc_req_read *req1 = (void *)p1;
    struct rtc_resp_read *resp1 = (void *)(p1 + 256);
    req1->msg_type = VIRTIO_RTC_REQ_READ;
    req1->clock_id = 0;

    struct rtc_req_read *req2 = (void *)p2;
    struct rtc_resp_read *resp2 = (void *)(p2 + 256);
    req2->msg_type = VIRTIO_RTC_REQ_READ;
    req2->clock_id = 0;

    /* Chain 1: desc 0 -> desc 1 */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req1),
                       (uint32_t)sizeof(*req1), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp1),
                       (uint32_t)sizeof(*resp1), VRING_DESC_F_WRITE, 0);

    /* Chain 2: desc 2 -> desc 3 */
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(req2),
                       (uint32_t)sizeof(*req2), VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(resp2),
                       (uint32_t)sizeof(*resp2), VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RTC0025, VIRTIO_PCI_DEVICE_RTC, test_rtc_concurrent_reads,
              "Two READ_TIME requests in one batch",
              VIRTIO_SPEC_V1_4, "5.23.6");
