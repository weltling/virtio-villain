/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0073: pci_common_cfg_3byte_access
 *
 * Spec 4.1 only defines aligned 1, 2, 4 and 8 byte reads of the
 * common configuration. A 3 byte read is undefined. Issue back
 * to back unaligned reads with byte width and verify the device
 * continues to honor a final aligned read of num_queues.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_3byte(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;

    volatile uint8_t *p = (volatile uint8_t *)dev->common;
    uint8_t a = p[0];
    uint8_t b = p[1];
    uint8_t c = p[2];
    (void)a; (void)b; (void)c;

    __sync_synchronize();
    uint16_t nq = dev->common->num_queues;
    if (nq == 0xFFFF)
        TFAIL("nq == 0xFFFF");

    if (dev->common->device_status == 0)
        TWEDGED("dev->common->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST(PCI0073, VIRTIO_PCI_DEVICE_BLK, test_pci_3byte,
              "Three byte split reads of common cfg",
              VIRTIO_SPEC_V1_2, "4.1");
