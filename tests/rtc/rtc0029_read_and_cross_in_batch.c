/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0029: READ_TIME and READ_CROSS submitted in the same avail
 * batch.
 *
 * Spec 5.23.6: the device must independently process heterogeneous
 * requests on the requestq. Push a READ_TIME chain and a
 * READ_CROSS chain in the same batch and verify both complete
 * without confusion between message types.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rtc_read_and_cross(struct virtio_dev *dev,
                                             struct vring *vr)
{
    uint8_t *p1 = vv_alloc_pages(1);
    uint8_t *p2 = vv_alloc_pages(1);
    memset(p1, 0, 4096);
    memset(p2, 0, 4096);

    struct rtc_req_read *r1 = (void *)p1;
    r1->msg_type = VIRTIO_RTC_REQ_READ;
    r1->clock_id = 0;

    struct rtc_req_read_cross *r2 = (void *)p2;
    r2->msg_type = VIRTIO_RTC_REQ_READ_CROSS;
    r2->clock_id = 0;
    r2->hw_counter = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(p1), sizeof(*r1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(p1) + 256,
                       sizeof(struct rtc_resp_read),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(p2), sizeof(*r2),
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(p2) + 256, 32,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(RTC0029, VIRTIO_PCI_DEVICE_RTC, test_rtc_read_and_cross,
              "READ_TIME and READ_CROSS in the same batch",
              VIRTIO_SPEC_V1_4, "5.23.6");
