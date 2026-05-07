/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0032: queue_enable cleared by device reset
 *
 * Spec 4.1.4.3.2 says writing 0 to device_status MUST reset the
 * device. After reset, every queue_enable register must read 0.
 * A VMM that leaves queue_enable=1 across a reset will allow the
 * driver to skip queue setup on reinit and operate on stale state.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_queue_enable_reset_clears(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    uint16_t nq = cfg->num_queues;
    if (nq == 0)
        return TEST_SKIP;
    if (nq > 16)
        nq = 16;

    /* Confirm queue 0 is enabled before reset (harness enabled it) */
    cfg->queue_select = 0;
    __sync_synchronize();
    if (cfg->queue_enable != 1)
        return TEST_SKIP;

    /* Reset */
    cfg->device_status = 0;
    __sync_synchronize();

    int tries = 200;
    while (tries-- > 0 && cfg->device_status != 0)
        usleep(1000);
    if (cfg->device_status != 0)
        TWEDGED("cfg->device_status != 0");

    /* Every queue_enable must be 0 after reset */
    for (uint16_t q = 0; q < nq; q++) {
        cfg->queue_select = q;
        __sync_synchronize();
        if (cfg->queue_enable != 0)
            TFAIL("cfg->queue_enable != 0");
    }

    return TEST_PASS;
}

REGISTER_TEST(S0032, VIRTIO_PCI_DEVICE_BLK, test_queue_enable_reset_clears,
              "queue_enable reads 0 for all queues after device reset",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
