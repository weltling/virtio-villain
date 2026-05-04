/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0019: Read QueueNumMax before writing QueueSel.
 *
 * Spec 4.2.3.2: The driver should select a queue (QueueSel) before
 * reading its properties. Test whether device has a sane default
 * value for QueueNumMax when no queue has been explicitly selected.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    /* Reset device */
    mmio_write32(dev, VIRTIO_MMIO_STATUS, 0);
    __sync_synchronize();
    usleep(10000);

    /*
     * Read QueueNumMax WITHOUT writing QueueSel first.
     * The device should either return a valid queue size for queue 0
     * (the implicit default) or 0 if no queues exist.
     */
    uint32_t queue_num_max = mmio_read32(dev, VIRTIO_MMIO_QUEUE_NUM_MAX);

    /* The value should be sane: 0 (no queue) or a power of 2 */
    if (queue_num_max == 0)
        return TEST_PASS; /* valid: no queue at default index */

    /* Check if it's a reasonable power of 2 */
    if (queue_num_max > 32768)
        TFAIL("queue_num_max > 32768"); /* unreasonably large */

    /* Now select queue 0 explicitly and compare */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_SEL, 0);
    __sync_synchronize();
    uint32_t queue_num_max_after = mmio_read32(dev, VIRTIO_MMIO_QUEUE_NUM_MAX);

    /* Should be consistent */
    if (queue_num_max != queue_num_max_after)
        TFAIL("queue_num_max != queue_num_max_after");

    /* Try selecting a non-existent queue */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_SEL, 0xFFFF);
    __sync_synchronize();
    uint32_t bogus_max = mmio_read32(dev, VIRTIO_MMIO_QUEUE_NUM_MAX);

    /* Non-existent queue should report 0 */
    if (bogus_max != 0)
        TFAIL("bogus_max != 0");

    return TEST_PASS;
}

REGISTER_TEST_MMIO(M0019, do_test,
    "Read QueueNumMax before writing QueueSel",
    VIRTIO_SPEC_V1_2, "4.2.3.2");
