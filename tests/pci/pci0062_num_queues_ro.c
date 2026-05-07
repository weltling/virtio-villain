/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0062: write num_queues read-only register.
 *
 * Spec 4.1.4.3.1: num_queues is read-only for the driver. Writing
 * to it must not change its value.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_num_queues_ro(struct virtio_dev *dev,
                                            struct vring *vr)
{
    (void)vr;
    uint16_t before = dev->common->num_queues;
    dev->common->num_queues = (uint16_t)(before + 1);
    __sync_synchronize();
    uint16_t after = dev->common->num_queues;
    if (after != before)
        TFAIL("after != before");
    return TEST_PASS;
}

REGISTER_TEST(PCI0062, 0, test_pci_num_queues_ro,
              "num_queues is read-only",
              VIRTIO_SPEC_V1_2, "4.1.4.3.1");
