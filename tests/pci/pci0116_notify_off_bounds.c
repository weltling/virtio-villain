/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0116: queue_notify_off is within the notify region.
 *
 * Spec 4.1.4.4: queue_notify_off multiplied by notify_off_multiplier
 * gives the offset of the queue's notification address within the
 * notify BAR region. Verify the resulting offset for queue 0 is
 * within the mapped notify region bounds.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_pci_notify_off(struct virtio_dev *dev,
                                         struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->queue_select = 0;
    __sync_synchronize();
    uint16_t noff = cfg->queue_notify_off;

    uint32_t byte_off = (uint32_t)noff * dev->notify_off_multiplier;

    /* The notification word (2 bytes) must fit within the region */
    if (byte_off + 2 > dev->notify_length)
        TFAIL("notify offset %u + 2 exceeds region length %u",
              byte_off, dev->notify_length);

    return TEST_PASS;
}

REGISTER_TEST(PCI0116, VIRTIO_PCI_DEVICE_BLK, test_pci_notify_off,
              "queue_notify_off resolves within notify region",
              VIRTIO_SPEC_V1_2, "4.1.4.4");
