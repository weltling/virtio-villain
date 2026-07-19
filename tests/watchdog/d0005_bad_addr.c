/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0005: Watchdog ping with out-of-range address.
 *
 * Submit a writable ping descriptor whose addr points to
 * 0xFFFFFFFFFFFF0000, outside any valid guest memory region. The
 * device must fail the write attempt safely rather than pass an
 * unchecked address into the host's translation path.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_watchdog_bad_addr(struct virtio_dev *dev,
                                            struct vring *vr)
{
    vring_raw_set_desc(vr, 0, 0xFFFFFFFFFFFF0000ULL, 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(D0005, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_bad_addr,
              "Watchdog ping with out of range address",
              VIRTIO_SPEC_V1_2, "-");
