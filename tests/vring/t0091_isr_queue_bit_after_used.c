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
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

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

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    __sync_synchronize();
    uint8_t isr = *dev->isr;

    if (!(isr & VIRTIO_PCI_ISR_QUEUE))
        TREJECT("!(isr & VIRTIO_PCI_ISR_QUEUE)");

    return TEST_PASS;
}

REGISTER_TEST(T0091, VIRTIO_PCI_DEVICE_BLK, test_isr_queue_bit_after_used,
              "ISR queue bit is set after used ring advances",
              VIRTIO_SPEC_V1_2, "4.1.4.5");
