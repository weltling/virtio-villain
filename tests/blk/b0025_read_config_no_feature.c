/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0025: blk_read_config_no_feature
 *
 * Access device-specific config space bytes beyond the base capacity
 * field without having negotiated the features that gate them.
 * We read raw bytes from the BAR area past common_cfg. If the device
 * has a device_cfg capability, those bytes are config fields gated by
 * un-negotiated features. Must not crash the VMM.
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

    /* Read well beyond common_cfg struct into adjacent BAR space */
    volatile uint8_t *base = (volatile uint8_t *)dev->common;
    volatile uint8_t val;
    for (int i = 0; i < 256; i++) {
        val = base[i];
        (void)val;
    }

    /* Survival is enough */
    return TEST_PASS;
}

REGISTER_TEST(B0025, VIRTIO_PCI_DEVICE_BLK, test_blk_read_config_no_feature,
              "Read config fields gated by un-negotiated features",
              VIRTIO_SPEC_V1_2, "5.2.5.1");
