/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0030: RTC CFG response descriptor in device MMIO BAR.
 *
 * Spec draft RTC.5: The RTC CFG request returns a writable
 * response. Submit a CFG whose writable response descriptor
 * addr points at the device's own common configuration BAR
 * rather than RAM. A device that writes the status and clock
 * count through the generic memory API without validating the
 * target region can wedge or corrupt its own registers. The
 * device must reject or handle the non RAM target cleanly.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

#define VIRTIO_RTC_REQ_CFG 0x1000

static test_result_t test_rtc_resp_in_bar(struct virtio_dev *dev,
                                          struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 64);
    struct rtc_req_head *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_CFG;

    uint64_t mmio_phys = dev->common_phys;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, mmio_phys, sizeof(struct rtc_resp_cfg),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RTC0030, VIRTIO_PCI_DEVICE_RTC, test_rtc_resp_in_bar,
              "RTC CFG response descriptor in device MMIO BAR",
              VIRTIO_SPEC_V1_4, "RTC.5");
