/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0094: num_queues is read only from the driver.
 *
 * Spec 4.1.4.3: num_queues is device written. A driver write
 * must be ignored. Save, write a different value, read back;
 * the value must remain.
 */
#include "tests/test.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t orig = cfg->num_queues;
    cfg->num_queues = (uint16_t)(orig + 13);
    __sync_synchronize();
    uint16_t now = cfg->num_queues;
    if (now != orig)
        TFAIL("num_queues writable: was %u, now %u", orig, now);
    return TEST_PASS;
}

REGISTER_TEST(PCI0094, VIRTIO_PCI_DEVICE_BLK, test,
              "num_queues ignores driver writes",
              VIRTIO_SPEC_V1_4, "4.1.4.3");
