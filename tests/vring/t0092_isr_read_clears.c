/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0092: ISR read clears the queue bit
 *
 * Spec 4.1.4.5 says reading the ISR register clears it as a side
 * effect. Drive a request to set the queue bit, read ISR once and
 * confirm the bit is set, then read ISR a second time and confirm
 * the queue bit is now clear with no new activity in between.
 */
#include "lib/virtio_test.h"

static test_result_t test_isr_read_clears(struct virtio_dev *dev,
                                          struct vring *vr)
{
    if (!dev->isr)
        return TEST_SKIP;

    vr->avail->flags = 0;
    __sync_synchronize();
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
    uint8_t first = *dev->isr;
    if (!(first & VIRTIO_PCI_ISR_QUEUE))
        TREJECT("!(first & VIRTIO_PCI_ISR_QUEUE)");

    __sync_synchronize();
    uint8_t second = *dev->isr;
    if (second & VIRTIO_PCI_ISR_QUEUE)
        TFAIL("second & VIRTIO_PCI_ISR_QUEUE");

    return TEST_PASS;
}

REGISTER_TEST_FLAGS(T0092, VIRTIO_PCI_DEVICE_BLK, test_isr_read_clears,
              "ISR read clears the queue bit",
              VIRTIO_SPEC_V1_2, "4.1.4.5",
              TEST_FLAG_NEEDS_ISR);
