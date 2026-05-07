/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0069: per queue notification offset multiplier honoured
 *
 * Spec 4.1.4.4 says queue_notify_off times notify_off_multiplier
 * gives the byte offset within the notification BAR for a queue.
 * A device that hardcodes the offset to zero will still let some
 * tests pass when only one queue is in use, but for queues that
 * advertise distinct notify_off values the writes must reach the
 * intended queue. Read every queue's notify_off and verify the
 * derived offset stays within the notification region.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

static test_result_t test_pci_notify_off_in_range(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    uint16_t nq = cfg->num_queues;
    if (nq == 0 || nq > 32)
        return TEST_SKIP;

    uint32_t mul = dev->notify_off_multiplier;
    uint32_t span = dev->notify_length;

    for (uint16_t q = 0; q < nq; q++) {
        cfg->queue_select = q;
        __sync_synchronize();
        if (cfg->queue_size == 0)
            continue;
        uint16_t noff = cfg->queue_notify_off;
        uint64_t byte_off = (uint64_t)noff * mul;
        if (byte_off + 2 > span)
            TFAIL("byte_off + 2 > span");
    }

    cfg->queue_select = 0;
    __sync_synchronize();
    return TEST_PASS;
}

REGISTER_TEST(PCI0069, VIRTIO_PCI_DEVICE_BLK, test_pci_notify_off_in_range,
              "queue_notify_off times multiplier stays in notify region",
              VIRTIO_SPEC_V1_2, "4.1.4.4");
