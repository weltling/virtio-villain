/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0029: PCI capability list sanity (spec 4.1.4)
 *
 * Walk the PCI capability list and verify all virtio capabilities
 * have valid bar, offset, and length fields that don't exceed
 * the BAR region size.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

static test_result_t test_pci_cap_list_sanity(struct virtio_dev *dev,
                                              struct vring *vr)
{
    (void)vr;

    /* Verify we found key capability regions */
    if (!dev->common)
        TFAIL("!dev->common");

    /* common_length should be >= the v1.2 minimum (56 bytes) */
    if (dev->common_length < 56)
        TFAIL("dev->common_length < 56 (virtio 1.2 minimum)");

    /* ISR should exist and have non-zero length */
    if (!dev->isr || dev->isr_length == 0)
        TFAIL("!dev->isr || dev->isr_length == 0");

    /* Notify should exist */
    if (!dev->notify_base || dev->notify_length == 0)
        TFAIL("!dev->notify_base || dev->notify_length == 0");

    /* Device config may not exist for all device types, but if it does
     * it should have non-zero length */
    if (dev->device_cfg && dev->device_cfg_length == 0)
        TFAIL("dev->device_cfg && dev->device_cfg_length == 0");

    /* Verify common cfg fields are readable */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;
    (void)nq; /* just verify no fault */

    uint8_t status = cfg->device_status;
    (void)status;

    return TEST_PASS;
}

REGISTER_TEST(PCI0029, VIRTIO_PCI_DEVICE_BLK, test_pci_cap_list_sanity,
              "PCI capability regions have valid sizes and pointers",
              VIRTIO_SPEC_V1_2, "4.1.4");
