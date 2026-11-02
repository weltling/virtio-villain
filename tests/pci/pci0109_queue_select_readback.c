/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0109: queue_select readback matches written value.
 *
 * Spec 4.1.4.3.1: Writing queue_select selects the current queue.
 * Reading it back must return the same value. Write 0, read back,
 * verify 0.
 */
#include "lib/virtio_test.h"

static test_result_t test_pci_qsel_readback(struct virtio_dev *dev)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->queue_select = 0;
    __sync_synchronize();
    uint16_t rb = cfg->queue_select;

    if (rb != 0)
        TFAIL("queue_select readback %u after writing 0", rb);

    return TEST_PASS;
}

REGISTER_TEST_DEV(PCI0109, VIRTIO_PCI_DEVICE_BLK, test_pci_qsel_readback,
                  "queue_select readback matches written value",
                  VIRTIO_SPEC_V1_2, "4.1.4.3.1");
