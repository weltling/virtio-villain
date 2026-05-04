/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0004: Watchdog ping with zero-length descriptor.
 *
 * Submit a writable descriptor with zero length to the watchdog.
 * The device must handle the edge case without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_watchdog_zero_len(struct virtio_dev *dev,
                                            struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    /* Zero-length writable descriptor */
    vring_raw_set_desc(vr, 0, buf_phys, 0, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(D0004, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_zero_len,
              "Watchdog ping with zero-length descriptor",
              VIRTIO_SPEC_V1_2, "5.16");
