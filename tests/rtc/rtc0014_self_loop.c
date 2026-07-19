/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0014: RTC chain self-loop.
 *
 * Build a chain whose head sets NEXT and points back at itself.
 * The device must detect the cycle.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <stdint.h>

static test_result_t test_rtc_self_loop(struct virtio_dev *dev,
                                        struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t base = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, base, 8, VRING_DESC_F_NEXT, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RTC0014, VIRTIO_PCI_DEVICE_RTC, test_rtc_self_loop,
              "RTC chain self-loop",
              VIRTIO_SPEC_V1_4, "5.23.6");
