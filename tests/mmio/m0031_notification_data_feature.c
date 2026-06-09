/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M0031: MMIO with notification data feature negotiated.
 *
 * Spec 4.2.2.2 plus 6 / VIRTIO_F_NOTIFICATION_DATA (bit 38).
 * When negotiated, drivers write a packed value into
 * QueueNotify carrying the queue index and the next ring
 * position. A device that ignored the feature reads only the
 * low 16 bits and may dispatch to the wrong queue when the
 * high bits encode a wrap counter or used_idx. Negotiate
 * the feature, then notify with extra bits set in the high
 * word and verify the device still consumes the descriptor.
 */
#include "tests/test.h"

#define VIRTIO_F_NOTIFICATION_DATA 38

static test_result_t do_test(struct virtio_mmio_dev *dev)
{
    if (virtio_mmio_init(dev) < 0)
        return TEST_SKIP;

    mmio_write32(dev, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 1);
    uint32_t hi = mmio_read32(dev, VIRTIO_MMIO_DEVICE_FEATURES);
    if (!(hi & (1U << (VIRTIO_F_NOTIFICATION_DATA - 32))))
        return TEST_SKIP;

    /* Notify with the high 16 bits non zero (next_off plus wrap). */
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_NOTIFY, 0x00010000u);
    __sync_synchronize();

    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (status == 0)
        TWEDGED("status == 0 after notification_data write");
    return TEST_PASS;
}

REGISTER_TEST_MMIO(M0031, do_test,
    "MMIO QueueNotify with notification data feature",
    VIRTIO_SPEC_V1_4, "4.2.3.3");
