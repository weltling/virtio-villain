/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0024: mmio_queue_reset_readback
 *
 * Write 1 to QueueReset, then read QueueReset back. Spec v1.3
 * 4.2.2.2: reading QueueReset returns 1 while the reset is in
 * progress, and 0 once complete.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    /* Select queue 0 */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_SEL, 0);
    __sync_synchronize();

    uint32_t qs = mmio_read32(dev, VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (qs == 0)
        return TEST_SKIP;

    /* Trigger queue reset */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_RESET, 1);
    __sync_synchronize();

    /* Read back. The value should be 1 (in progress) or 0 (done).
     * Any other value is invalid. */
    uint32_t val = mmio_read32(dev, VIRTIO_MMIO_QUEUE_RESET);
    if (val != 0 && val != 1)
        TFAIL("val != 0 && val != 1");

    return TEST_PASS;
}

REGISTER_TEST_MMIO(M0024, do_test,
    "QueueReset readback shows valid state",
    VIRTIO_SPEC_V1_3, "4.2.2.2");
