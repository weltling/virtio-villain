/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0011: Watchdog ping with all reserved descriptor flag bits set.
 *
 * Submit a ping descriptor with VRING_DESC_F_WRITE plus every
 * reserved bit in the flags field set. Per spec 2.7.5 reserved
 * bits MUST be zero; the device must either ignore the unknown
 * bits or reject the descriptor cleanly.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_watchdog_all_flags(struct virtio_dev *dev,
                                             struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    /* WRITE bit plus every reserved bit in the 16 bit flags field. */
    uint16_t flags = (uint16_t)(VRING_DESC_F_WRITE | 0xFFF8);

    vring_raw_set_desc(vr, 0, buf_phys, 1, flags, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(D0011, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_all_flags,
              "Watchdog ping with all reserved flags set",
              VIRTIO_SPEC_V1_2, "2.7.5");
