/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0034: watchdog rapid ping burst with no delay.
 *
 * Submit 8 ping descriptors in the avail ring simultaneously
 * with a single kick. The device must process all of them without
 * dropping any. Tests the device handles burst notifications under
 * load where no inter-ping delay exists.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define BURST_COUNT 8

static test_result_t test_watchdog_rapid_ping(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *bufs[BURST_COUNT];
    for (int i = 0; i < BURST_COUNT; i++) {
        bufs[i] = vv_alloc_pages(1);
        *bufs[i] = 0;
        vring_raw_set_desc(vr, i, vv_virt_to_phys(bufs[i]), 1,
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, i, i);
    }
    vring_raw_set_avail_idx(vr, BURST_COUNT);

    return vv_kick_and_wait_n(dev, vr, 0, BURST_COUNT, VV_TIMEOUT_MS);
}

REGISTER_TEST(D0034, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_rapid_ping,
              "Rapid ping burst of 8 with single kick",
              VIRTIO_SPEC_V1_4, "-");
