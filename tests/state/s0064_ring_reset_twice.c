/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0064: ring_reset_twice
 *
 * Reset the same queue twice in succession without re-enabling in
 * between. The device must handle the idempotent reset gracefully
 * without crashing. Spec 2.2.1 does not prohibit repeated resets.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

#define VIRTIO_F_RING_RESET 40

static test_result_t test_ring_reset_twice(struct virtio_dev *dev,
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

    cfg->queue_select = 0;
    __sync_synchronize();

    if (!cfg->queue_enable)
        return TEST_SKIP;

    /* First reset */
    cfg->queue_enable = 0;
    __sync_synchronize();
    usleep(50000);

    if (cfg->queue_enable != 0)
        TFAIL("cfg->queue_enable != 0");

    /* Second reset (queue already disabled) */
    cfg->queue_enable = 0;
    __sync_synchronize();
    usleep(50000);

    /* Must still be disabled, device must not crash */
    if (cfg->queue_enable != 0)
        TFAIL("cfg->queue_enable != 0");
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(S0064, VIRTIO_PCI_DEVICE_BLK, test_ring_reset_twice,
              "Reset same queue twice without re-enable",
              VIRTIO_SPEC_V1_3, "2.2.1");
