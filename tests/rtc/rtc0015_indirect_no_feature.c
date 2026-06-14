/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0015: RTC indirect without negotiated feature.
 *
 * Submit a requestq descriptor with VRING_DESC_F_INDIRECT while
 * VIRTIO_F_INDIRECT_DESC was not negotiated. Per spec 2.7.7 the
 * device must reject.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <stdint.h>

#define VIRTIO_RTC_REQ_CFG 0x1000

static test_result_t test_rtc_indirect_no_feature(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct vring_desc *itab = vv_alloc_pages(1);
    uint64_t itab_phys = vv_virt_to_phys(itab);
    uint64_t payload_phys = itab_phys + 256;
    struct rtc_req_head *req = (void *)((uint8_t *)itab + 256);
    req->msg_type = VIRTIO_RTC_REQ_CFG;

    itab[0].addr  = payload_phys;
    itab[0].len   = sizeof(*req);
    itab[0].flags = VRING_DESC_F_NEXT;
    itab[0].next  = 1;
    itab[1].addr  = payload_phys + 64;
    itab[1].len   = 16;
    itab[1].flags = VRING_DESC_F_WRITE;
    itab[1].next  = 0;

    vring_raw_set_desc(vr, 0, itab_phys, 2 * sizeof(*itab),
                       VRING_DESC_F_INDIRECT, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RTC0015, VIRTIO_PCI_DEVICE_RTC, test_rtc_indirect_no_feature,
              "RTC indirect without feature",
              VIRTIO_SPEC_V1_4, "2.7.7");
