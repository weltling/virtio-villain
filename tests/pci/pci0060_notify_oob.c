/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0060: notify with select beyond num_queues.
 *
 * Spec 4.1.4.4: the notify region is sized to cover all queues.
 * Writing a queue index beyond num_queues to the notify region
 * must not crash the VMM.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_notify_oob(struct virtio_dev *dev,
                                         struct vring *vr)
{
    (void)vr;
    uint16_t nq = dev->common->num_queues;
    uint32_t off = (uint32_t)nq * dev->notify_off_multiplier;
    if (!dev->notify_base)
        return TEST_SKIP;
    *(volatile uint16_t *)((uint8_t *)dev->notify_base + off) =
        (uint16_t)(nq + 50);
    __sync_synchronize();
    return TEST_PASS;
}

REGISTER_TEST(PCI0060, 0, test_pci_notify_oob,
              "Notify with queue index beyond num_queues",
              VIRTIO_SPEC_V1_2, "4.1.4.4");
