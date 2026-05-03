/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0013: QueueReady=1 without setting addresses (spec 4.2.3.2)
 *
 * The driver should configure QueueNum, QueueDesc, QueueDriver, and
 * QueueDevice before setting QueueReady. This test sets QueueReady
 * without providing any addresses. The VMM must not crash.
 */
#include "tests/test.h"

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    if (virtio_mmio_init(dev) < 0)
        return TEST_SKIP;

    /* Select queue 0 */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_SEL, 0);
    __sync_synchronize();

    uint32_t max = mmio_read32(dev, VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max == 0)
        return TEST_SKIP;

    /* Set QueueNum but leave all addresses at 0 */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_NUM, 16);
    __sync_synchronize();

    /* Set QueueReady without addresses - violation */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_READY, 1);
    __sync_synchronize();

    /* Try to kick this improperly configured queue */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_NOTIFY, 0);
    __sync_synchronize();

    usleep(50000);

    /* Verify device still alive */
    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status == 0)
        TWEDGED("status == 0");

    TREJECT("no device response within timeout");
}

REGISTER_TEST_MMIO(M0013, do_test,
    "QueueReady without setting queue addresses",
    VIRTIO_SPEC_V1_2, "4.2.3.2");
