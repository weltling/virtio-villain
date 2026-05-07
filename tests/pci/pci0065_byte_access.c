/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0065: 8-bit access to 32-bit register.
 *
 * Spec 4.1.3.1 says common config registers should be accessed
 * with their natural width. Read device_feature one byte at a
 * time and check the assembled value matches the 32-bit read.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_byte_access(struct virtio_dev *dev,
                                          struct vring *vr)
{
    (void)vr;
    dev->common->device_feature_select = 0;
    __sync_synchronize();
    uint32_t w = dev->common->device_feature;
    volatile uint8_t *p = (volatile uint8_t *)&dev->common->device_feature;
    uint32_t b = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                 ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    if (w != b)
        TFAIL("w != b");
    return TEST_PASS;
}

REGISTER_TEST(PCI0065, 0, test_pci_byte_access,
              "byte-wise read of 32-bit register",
              VIRTIO_SPEC_V1_2, "4.1.3.1");
