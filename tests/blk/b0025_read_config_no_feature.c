/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0025: blk_read_config_no_feature
 *
 * Access device-specific config space bytes beyond the base capacity
 * field without having negotiated the features that gate them.
 * The gated fields live in the device config region, so read every
 * mapped byte of it. Must not crash the VMM.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_read_config_no_feature(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    (void)vr;

    /*
     * Read common_cfg fields that are normally only meaningful after
     * feature negotiation: config_generation, queue_notify_off, etc.
     * The harness negotiated zero features, so any feature-gated
     * behavior triggered by reading these is a VMM bug.
     */
    volatile uint8_t gen = dev->common->config_generation;
    (void)gen;

    /* Read the device config in aligned 32-bit words bounded to the
     * mapped region. A byte wide scan of this region aborts the guest
     * on AArch64, where an aligned word read works. */
    if (dev->device_cfg && dev->device_cfg_length >= 4) {
        volatile uint32_t *cfg = (volatile uint32_t *)dev->device_cfg;
        volatile uint32_t val;
        for (uint32_t i = 0; i + 4 <= dev->device_cfg_length; i += 4) {
            val = cfg[i / 4];
            (void)val;
        }
    }

    /* Survival is enough */
    return TEST_PASS;
}

REGISTER_TEST(B0025, VIRTIO_PCI_DEVICE_BLK, test_blk_read_config_no_feature,
              "Read config fields gated by un-negotiated features",
              VIRTIO_SPEC_V1_2, "5.2.5.1");
