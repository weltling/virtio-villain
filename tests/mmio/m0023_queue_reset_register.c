/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0023: mmio_queue_reset_register
 *
 * Write 1 to QueueReset (offset 0x0c0) while a queue is enabled.
 * Spec v1.3 4.2.2.2: when VIRTIO_F_RING_RESET is negotiated,
 * writing 1 to this register resets the selected queue. After
 * reset, QueueReady must read 0.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    /* Select queue 0 */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_SEL, 0);
    __sync_synchronize();

    /* Check queue is available */
    uint32_t qs = mmio_read32(dev, VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (qs == 0)
        return TEST_SKIP;

    /* Write QueueReset = 1 */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_RESET, 1);
    __sync_synchronize();

    /* After reset, QueueReady must be 0 */
    uint32_t ready = mmio_read32(dev, VIRTIO_MMIO_QUEUE_READY);
    if (ready != 0)
        TFAIL("ready != 0");

    return TEST_PASS;
}

REGISTER_TEST_MMIO(M0023, do_test,
    "QueueReset register resets selected queue",
    VIRTIO_SPEC_V1_3, "4.2.2.2");
