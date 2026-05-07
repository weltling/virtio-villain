/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0052: queue_select out of range.
 *
 * Write a queue_select value larger than num_queues. Spec 4.1.4.3
 * says the driver MUST NOT, but a robust VMM must clamp or ignore
 * rather than index out of bounds.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_qsel_oob(struct virtio_dev *dev,
                                       struct vring *vr)
{
    (void)vr;
    uint16_t nq = dev->common->num_queues;
    dev->common->queue_select = (uint16_t)(nq + 1024);
    __sync_synchronize();
    (void)dev->common->queue_size;
    dev->common->queue_select = 0;
    return TEST_PASS;
}

REGISTER_TEST(PCI0052, 0, test_pci_qsel_oob,
              "queue_select beyond num_queues",
              VIRTIO_SPEC_V1_2, "4.1.4.3");
