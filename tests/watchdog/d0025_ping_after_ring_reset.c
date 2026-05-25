/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0025: Watchdog ping after per queue ring reset.
 *
 * Spec 2.2.1: With VIRTIO_F_RING_RESET negotiated, the driver may
 * reset an individual queue while the device remains in DRIVER_OK.
 * After the reset, a fresh ping submitted on the reattached queue
 * must complete. Tests that the device worker thread survives a
 * single queue reset and resumes processing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_F_RING_RESET 40

static test_result_t test_watchdog_ping_after_ring_reset(struct virtio_dev *dev,
                                                         struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = VIRTIO_F_RING_RESET / 32;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << (VIRTIO_F_RING_RESET % 32))))
        return TEST_SKIP;

    /* Disable the queue (ring reset under RING_RESET) */
    cfg->queue_select = 0;
    __sync_synchronize();
    cfg->queue_enable = 0;
    __sync_synchronize();
    usleep(50000);

    /* Re-enable the queue */
    cfg->queue_enable = 1;
    __sync_synchronize();
    usleep(50000);

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    /* Submit a fresh ping */
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 64);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 64,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(D0025, VIRTIO_PCI_DEVICE_WATCHDOG,
              test_watchdog_ping_after_ring_reset,
              "Watchdog ping after per queue ring reset",
              VIRTIO_SPEC_V1_3, "2.2.1");
