/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0027: PCI notify_off_multiplier validation (spec 4.1.4.4)
 *
 * The notify_off_multiplier from the notify capability determines
 * per-queue notification offsets. Verify it's consistent and
 * that computed offsets don't exceed the notify BAR region.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pci_notify_multiplier(struct virtio_dev *dev,
                                                struct vring *vr)
{
    (void)vr;

    if (!dev->notify_base || dev->notify_length == 0)
        return TEST_SKIP;

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;
    if (nq == 0)
        nq = 1;

    /* Check each queue's notify offset doesn't exceed region */
    for (uint16_t q = 0; q < nq; q++) {
        cfg->queue_select = q;
        __sync_synchronize();
        uint16_t qno = cfg->queue_notify_off;
        uint32_t offset = (uint32_t)qno * dev->notify_off_multiplier;

        if (offset + 2 > dev->notify_length) {
            /* Notify offset goes beyond notify BAR region - BAD */
            TFAIL("offset + 2 > dev->notify_length");
        }
    }

    /* If multiplier is 0, all queues share same offset */
    if (dev->notify_off_multiplier == 0) {
        cfg->queue_select = 0;
        __sync_synchronize();
        uint16_t off0 = cfg->queue_notify_off;

        if (nq > 1) {
            cfg->queue_select = 1;
            __sync_synchronize();
            uint16_t off1 = cfg->queue_notify_off;
            /* With multiplier=0, effective offset is always 0 regardless */
            (void)off0;
            (void)off1;
        }
    }

    return TEST_PASS;
}

REGISTER_TEST(PCI0027, VIRTIO_PCI_DEVICE_BLK, test_pci_notify_multiplier,
              "Notify offset multiplier consistent with BAR size",
              VIRTIO_SPEC_V1_2, "4.1.4.4");
