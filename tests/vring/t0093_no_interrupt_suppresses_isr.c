/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0093: NO_INTERRUPT in avail flags suppresses ISR queue bit
 *
 * Spec 2.7.7 lets the driver set VRING_AVAIL_F_NO_INTERRUPT in
 * avail->flags as a hint that interrupts for the queue are not
 * needed. The device should still process buffers but it must not
 * raise the queue interrupt. Submit a valid block read with the
 * suppression flag set, wait for completion, then check that ISR
 * queue bit is not set.
 */
#include "lib/virtio_test.h"

static test_result_t test_no_interrupt_suppresses_isr(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    if (!dev->isr)
        return TEST_SKIP;

    /* Drain ISR */
    (void)*dev->isr;
    __sync_synchronize();

    /* Set NO_INTERRUPT in avail flags */
    vr->avail->flags = VRING_AVAIL_F_NO_INTERRUPT;
    __sync_synchronize();

    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    *st = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    VV_TRY(vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS));

    __sync_synchronize();
    uint8_t isr = *dev->isr;

    /* Restore default */
    vr->avail->flags = 0;
    __sync_synchronize();

    if (isr & VIRTIO_PCI_ISR_QUEUE)
        TFAIL("isr & VIRTIO_PCI_ISR_QUEUE");

    return TEST_PASS;
}

REGISTER_TEST_FLAGS(T0093, VIRTIO_PCI_DEVICE_BLK, test_no_interrupt_suppresses_isr,
              "NO_INTERRUPT avail flag keeps ISR queue bit clear",
              VIRTIO_SPEC_V1_2, "2.7.7",
              TEST_FLAG_NEEDS_ISR);
