/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0091: ISR queue bit set after used ring advance
 *
 * Spec 4.1.4.5 says when the device produces a buffer it must
 * write 1 to the queue interrupt bit (bit 0) of the ISR register
 * unless interrupt suppression is in effect. Submit a valid block
 * read with interrupts enabled, wait for the used ring to advance,
 * then check ISR. The spec also says reading ISR clears it, so
 * read once into a local and assert.
 */
#include "lib/virtio_test.h"

static test_result_t test_isr_queue_bit_after_used(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    if (!dev->isr)
        return TEST_SKIP;

    /* Make sure interrupts are not suppressed */
    vr->avail->flags = 0;
    __sync_synchronize();

    /* Drain any pending ISR bits */
    (void)*dev->isr;
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

    if (!(isr & VIRTIO_PCI_ISR_QUEUE))
        TREJECT("!(isr & VIRTIO_PCI_ISR_QUEUE)");

    return TEST_PASS;
}

REGISTER_TEST_FLAGS(T0091, VIRTIO_PCI_DEVICE_BLK, test_isr_queue_bit_after_used,
              "ISR queue bit is set after used ring advances",
              VIRTIO_SPEC_V1_2, "4.1.4.5",
              TEST_FLAG_NEEDS_ISR);
