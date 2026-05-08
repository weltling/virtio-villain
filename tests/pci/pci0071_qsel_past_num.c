/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0071: queue_select past num_queues reads queue_size zero
 *
 * Spec 4.1.4.3.1 says queue_select selects one of the device's
 * virtqueues. Reading queue_size and queue_enable for selectors
 * at or beyond num_queues must read zero, since those queues do
 * not exist. Walk selectors num_queues through num_queues+7,
 * verify both read zero, then restore queue_select to 0 and read
 * queue_size to confirm the device still responds normally.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

static test_result_t test_pci_qsel_past_num(struct virtio_dev *dev,
                                            struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    uint16_t nq = cfg->num_queues;
    if (nq == 0)
        return TEST_SKIP;

    for (uint16_t s = nq; s < (uint16_t)(nq + 8); s++) {
        cfg->queue_select = s;
        __sync_synchronize();
        if (cfg->queue_size != 0)
            TFAIL("cfg->queue_size != 0");
        if (cfg->queue_enable != 0)
            TFAIL("cfg->queue_enable != 0");
    }

    cfg->queue_select = 0;
    __sync_synchronize();
    if (cfg->queue_size == 0)
        TFAIL("cfg->queue_size == 0");

    return TEST_PASS;
}

REGISTER_TEST(PCI0071, VIRTIO_PCI_DEVICE_BLK, test_pci_qsel_past_num,
              "queue_size and queue_enable read zero past num_queues",
              VIRTIO_SPEC_V1_2, "4.1.4.3.1");
