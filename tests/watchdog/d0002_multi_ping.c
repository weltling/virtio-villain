/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0002: Watchdog multiple pings.
 *
 * Submit multiple ping descriptors in sequence to verify the device
 * handles repeated keepalive operations.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_watchdog_multi_ping(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *bufs[4];
    for (int i = 0; i < 4; i++) {
        bufs[i] = vv_alloc_pages(1);
        *bufs[i] = 0;
        vring_raw_set_desc(vr, i, vv_virt_to_phys(bufs[i]), 1,
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, i, i);
    }
    vring_raw_set_avail_idx(vr, 4);

    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if (vr->used->idx >= 4)
            return TEST_PASS;
        elapsed += 10000;
    }

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(D0002, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_multi_ping,
              "Watchdog multiple pings in batch",
              VIRTIO_SPEC_V1_2, "-");
