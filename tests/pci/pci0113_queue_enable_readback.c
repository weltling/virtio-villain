/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0113: queue_enable readback is 1 after enabling.
 *
 * Spec 4.1.4.3.2: After writing 1 to queue_enable, reading it
 * back must return 1.
 */
#include "lib/virtio_test.h"

static test_result_t test_pci_qen_readback(struct virtio_dev *dev)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Queue 0 should already be enabled by the harness */
    cfg->queue_select = 0;
    __sync_synchronize();
    uint16_t en = cfg->queue_enable;

    if (en != 1)
        TFAIL("queue_enable readback %u, expected 1", en);

    return TEST_PASS;
}

REGISTER_TEST_DEV(PCI0113, VIRTIO_PCI_DEVICE_BLK, test_pci_qen_readback,
                  "queue_enable readback is 1 after enabling",
                  VIRTIO_SPEC_V1_2, "4.1.4.3.2");
