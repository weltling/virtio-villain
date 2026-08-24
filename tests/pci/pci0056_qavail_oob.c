/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0056: queue_avail outside guest RAM.
 *
 * Point queue_avail at 0xFFFFFFFFF0000000 then re-enable. The VMM
 * must reject the queue rather than walk a bogus mapping.
 *
 * Spec 4.1.4.3.2.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_qavail_oob(struct virtio_dev *dev,
                                         struct vring *vr)
{
    (void)vr;
    uint64_t saved = virtio_load64(&dev->common->queue_avail);
    dev->common->queue_select = 0;
    __sync_synchronize();
    virtio_store64(&dev->common->queue_avail, 0xFFFFFFFFF0000000ULL);
    __sync_synchronize();
    usleep(5000);
    virtio_store64(&dev->common->queue_avail, saved);
    return TEST_PASS;
}

REGISTER_TEST(PCI0056, 0, test_pci_qavail_oob,
              "queue_avail outside guest RAM",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
