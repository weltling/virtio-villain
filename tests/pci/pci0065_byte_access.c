/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0065: 8-bit access to a 32-bit common config register.
 *
 * Spec 4.1.3.1 requires the driver to access a 32-bit field with a
 * single 32-bit access, so a device may decode the common config by
 * register offset and return 0 for byte reads inside a 32-bit field.
 * This exercises byte-wide reads of device_feature to confirm the
 * device tolerates them, then verifies it still serves an aligned
 * read of num_queues.
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

    volatile uint8_t *p = (volatile uint8_t *)&dev->common->device_feature;
    uint8_t b0 = p[0];
    uint8_t b1 = p[1];
    uint8_t b2 = p[2];
    uint8_t b3 = p[3];
    (void)b0; (void)b1; (void)b2; (void)b3;
    __sync_synchronize();

    uint16_t nq = dev->common->num_queues;
    if (nq == 0xFFFF)
        TFAIL("nq == 0xFFFF");

    if (dev->common->device_status == 0)
        TWEDGED("dev->common->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST(PCI0065, 0, test_pci_byte_access,
              "byte-wise read of 32-bit register",
              VIRTIO_SPEC_V1_2, "4.1.3.1");
