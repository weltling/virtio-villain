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
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VRING_AVAIL_F_NO_INTERRUPT 1

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

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

    /* Restore default */
    vr->avail->flags = 0;
    __sync_synchronize();

    if (isr & VIRTIO_PCI_ISR_QUEUE)
        TFAIL("isr & VIRTIO_PCI_ISR_QUEUE");

    return TEST_PASS;
}

REGISTER_TEST(T0093, VIRTIO_PCI_DEVICE_BLK, test_no_interrupt_suppresses_isr,
              "NO_INTERRUPT avail flag keeps ISR queue bit clear",
              VIRTIO_SPEC_V1_2, "2.7.7");
