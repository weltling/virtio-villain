/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0097: device cfg field alignment requirements.
 *
 * Spec 4.1.4.6: device specific config fields are accessed
 * with their natural alignment. Verify a sequence of byte
 * wide reads matches a single 4 byte read.
 */
#include "tests/test.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    if (!dev->device_cfg || dev->device_cfg_length < 4)
        return TEST_SKIP;
    volatile uint8_t  *b = dev->device_cfg;
    volatile uint32_t *w = dev->device_cfg;
    uint32_t v = *w;
    uint32_t reassembled = (uint32_t)b[0] |
                           ((uint32_t)b[1] << 8) |
                           ((uint32_t)b[2] << 16) |
                           ((uint32_t)b[3] << 24);
    if (v != reassembled)
        TFAIL("byte reads do not match 32 bit read: %x vs %x",
              reassembled, v);
    return TEST_PASS;
}

REGISTER_TEST(PCI0097, VIRTIO_PCI_DEVICE_BLK, test,
              "Device config byte and word accesses agree",
              VIRTIO_SPEC_V1_4, "4.1.4.6");
