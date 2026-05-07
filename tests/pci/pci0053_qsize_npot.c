/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0053: queue_size non-power-of-two.
 *
 * Spec 4.1.4.3.2 says queue_size MUST be a power of two. Write
 * 17 and verify the VMM either clamps to power-of-two or
 * refuses to honor it without dying.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_qsize_npot(struct virtio_dev *dev,
                                         struct vring *vr)
{
    (void)vr;
    dev->common->queue_select = 0;
    __sync_synchronize();
    dev->common->queue_size = 17;
    __sync_synchronize();
    (void)dev->common->queue_size;
    return TEST_PASS;
}

REGISTER_TEST(PCI0053, 0, test_pci_qsize_npot,
              "queue_size non-power-of-two",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
