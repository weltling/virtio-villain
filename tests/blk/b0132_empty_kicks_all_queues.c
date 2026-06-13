/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0132: Empty kicks on all queues.
 *
 * Notify all available queues with no descriptors posted to any of
 * them. The device must handle this without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_blk_empty_kicks_all_queues(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;

    (void)vr;

    if (nq < 1)
        return TEST_SKIP;
    if (nq > 16)
        nq = 16;

    for (uint16_t q = 0; q < nq; q++) {
        cfg->queue_select = q;
        __sync_synchronize();
        virtio_pci_kick(dev, q);
    }

    usleep(200000);

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(B0132, VIRTIO_PCI_DEVICE_BLK, test_blk_empty_kicks_all_queues,
              "Empty kicks on all queues",
              VIRTIO_SPEC_V1_2, "5.2.6");
