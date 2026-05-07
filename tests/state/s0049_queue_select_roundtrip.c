/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0049: queue_select round trip across all queues
 *
 * Spec 4.1.4.3.2 makes queue_select an index that the driver writes
 * to choose which queue the per queue fields apply to. Writing each
 * valid index then reading back must yield the same value. As a
 * side effect each select must also expose a non zero queue_size
 * for queues the device actually has.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

static test_result_t test_queue_select_roundtrip(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    uint16_t nq = cfg->num_queues;
    if (nq == 0 || nq > 64)
        return TEST_SKIP;

    int seen_real = 0;

    for (uint16_t i = 0; i < nq; i++) {
        cfg->queue_select = i;
        __sync_synchronize();
        uint16_t v = cfg->queue_select;
        if (v != i)
            TFAIL("v != i");
        if (cfg->queue_size != 0)
            seen_real = 1;
    }

    if (!seen_real)
        TFAIL("!seen_real");

    /* Restore */
    cfg->queue_select = 0;
    __sync_synchronize();

    return TEST_PASS;
}

REGISTER_TEST(S0049, VIRTIO_PCI_DEVICE_BLK, test_queue_select_roundtrip,
              "queue_select round trip across every queue index",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
