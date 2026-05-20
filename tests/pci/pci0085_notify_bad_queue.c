/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0085: Queue notify with invalid queue index.
 *
 * Spec 4.1.5.2: Write a notification for a queue index that
 * exceeds the number of queues supported by the device. The
 * device must ignore the spurious notification rather than
 * indexing out of bounds.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pci_notify_bad_queue(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;

    /*
     * Write to the notify BAR with a queue index of 0xFFFF.
     * We use the notify offset for queue 0 but write an invalid
     * queue value.
     */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->queue_select = 0;
    uint16_t notify_off = cfg->queue_notify_off;

    volatile uint16_t *notify = (volatile uint16_t *)
        ((volatile uint8_t *)dev->notify_base +
         (uint32_t)notify_off * dev->notify_off_multiplier);

    *notify = 0xFFFF;
    __sync_synchronize();

    usleep(VV_TIMEOUT_MS * 1000);

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST(PCI0085, VIRTIO_PCI_DEVICE_BLK, test_pci_notify_bad_queue,
              "Queue notify with invalid queue index 0xFFFF",
              VIRTIO_SPEC_V1_2, "4.1.5.2");
