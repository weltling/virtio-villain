/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0060: ring_reset_single_queue
 *
 * Negotiate VIRTIO_F_RING_RESET, then reset a single virtqueue while
 * the device remains in DRIVER_OK. Spec 2.2.1: the driver can reset
 * a queue individually when VIRTIO_F_RING_RESET is negotiated.
 * After reset, queue state must return to the default.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_ring_reset_single(struct virtio_dev *dev,
                                            struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)vr;

    /* Check if RING_RESET is offered */
    cfg->device_feature_select = VIRTIO_F_RING_RESET / 32;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << (VIRTIO_F_RING_RESET % 32))))
        return TEST_SKIP;

    /* Select queue 0 */
    cfg->queue_select = 0;
    __sync_synchronize();

    uint16_t enable_before = cfg->queue_enable;
    if (!enable_before)
        return TEST_SKIP;

    /* Reset the queue via the queue_reset register (spec 2.6.1). */
    if (virtio_pci_queue_reset(dev, 0) < 0)
        TFAIL("queue_enable not cleared after reset");

    /* Verify queue is now disabled */
    uint16_t enable_after = cfg->queue_enable;
    if (enable_after != 0)
        TFAIL("enable_after != 0");

    /* Device must still be operational (not in reset) */
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(S0060, VIRTIO_PCI_DEVICE_BLK, test_ring_reset_single,
              "Reset single virtqueue with VIRTIO_F_RING_RESET",
              VIRTIO_SPEC_V1_3, "2.2.1");
