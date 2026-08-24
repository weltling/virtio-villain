/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0033: queue address registers cleared by device reset
 *
 * Spec 4.1.4.3.2 requires a device reset to bring the device back
 * to its initial state. Driver perspective is that queue_desc,
 * queue_driver and queue_device for every queue must read 0 after
 * a reset, so that the driver knows it has to program addresses
 * again before reenabling the queue.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_queue_addr_reset_clears(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    uint16_t nq = cfg->num_queues;
    if (nq == 0)
        return TEST_SKIP;
    if (nq > 16)
        nq = 16;

    /* Reset */
    cfg->device_status = 0;
    __sync_synchronize();

    int tries = 200;
    while (tries-- > 0 && cfg->device_status != 0)
        usleep(1000);
    if (cfg->device_status != 0)
        TWEDGED("cfg->device_status != 0");

    for (uint16_t q = 0; q < nq; q++) {
        cfg->queue_select = q;
        __sync_synchronize();
        if (virtio_load64(&cfg->queue_desc) != 0)
            TFAIL("cfg->queue_desc != 0");
        if (virtio_load64(&cfg->queue_avail) != 0)
            TFAIL("cfg->queue_avail != 0");
        if (virtio_load64(&cfg->queue_used) != 0)
            TFAIL("cfg->queue_used != 0");
    }

    return TEST_PASS;
}

REGISTER_TEST(S0033, VIRTIO_PCI_DEVICE_BLK, test_queue_addr_reset_clears,
              "queue_desc queue_driver queue_device read 0 after reset",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
