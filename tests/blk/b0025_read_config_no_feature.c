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

    /* Stay inside the mapped device config so the access is valid
     * device memory on every architecture. */
    if (dev->device_cfg && dev->device_cfg_length > 0) {
        volatile uint8_t *cfg = (volatile uint8_t *)dev->device_cfg;
        volatile uint8_t val;
        for (uint32_t i = 0; i < dev->device_cfg_length; i++) {
            val = cfg[i];
            (void)val;
        }
    }

    /* Survival is enough */
    return TEST_PASS;
}

REGISTER_TEST(B0025, VIRTIO_PCI_DEVICE_BLK, test_blk_read_config_no_feature,
              "Read config fields gated by un-negotiated features",
              VIRTIO_SPEC_V1_2, "5.2.5.1");
