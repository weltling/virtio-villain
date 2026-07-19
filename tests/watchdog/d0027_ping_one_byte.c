/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0027: Watchdog ping with a one byte writable descriptor.
 *
 * The watchdog only requires a tiny pong buffer.
 * Submit a single byte writable descriptor. The device must
 * accept the minimal buffer and complete the ping without
 * underrunning or rounding the length up to a larger size.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_watchdog_one_byte(struct virtio_dev *dev,
                                            struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    *buf = 0xA5;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(D0027, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_one_byte,
              "Watchdog ping with a one byte writable buffer",
              VIRTIO_SPEC_V1_2, "-");
