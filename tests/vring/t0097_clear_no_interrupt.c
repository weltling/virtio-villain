/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0097: clear NO_INTERRUPT and verify next request fires ISR
 *
 * Spec 2.7.7 makes VRING_AVAIL_F_NO_INTERRUPT a driver hint. When
 * the driver later clears the bit, subsequent completions must
 * raise the queue interrupt again. Submit a request with the bit
 * set, drain the ISR, then submit another with the bit cleared
 * and verify ISR queue bit is set.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

#define VRING_AVAIL_F_NO_INTERRUPT 1

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static int submit_read(struct virtio_dev *dev, struct vring *vr,
                       uint16_t base, uint16_t avail_slot)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    vring_raw_set_desc(vr, base, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, base + 1);
    vring_raw_set_desc(vr, base + 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, base + 2);
    vring_raw_set_desc(vr, base + 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, avail_slot, base);
    vring_raw_set_avail_idx(vr, avail_slot + 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS) == TEST_PASS ? 0 : -1;
}

static test_result_t test_clear_no_interrupt(struct virtio_dev *dev,
                                             struct vring *vr)
{
    if (!dev->isr)
        return TEST_SKIP;

    /* Suppress, submit, drain ISR */
    vr->avail->flags = VRING_AVAIL_F_NO_INTERRUPT;
    __sync_synchronize();
    if (submit_read(dev, vr, 0, 0) < 0)
        TFAIL("submit_read(dev, vr, 0, 0) < 0");
    (void)*dev->isr;
    __sync_synchronize();

    /* Clear the bit, submit again, ISR queue bit must be set */
    vr->avail->flags = 0;
    __sync_synchronize();
    if (submit_read(dev, vr, 3, 1) < 0)
        TFAIL("submit_read(dev, vr, 3, 1) < 0");

    __sync_synchronize();
    uint8_t isr = *dev->isr;
    if (!(isr & VIRTIO_PCI_ISR_QUEUE))
        TREJECT("!(isr & VIRTIO_PCI_ISR_QUEUE)");

    return TEST_PASS;
}

REGISTER_TEST(T0097, VIRTIO_PCI_DEVICE_BLK, test_clear_no_interrupt,
              "clearing NO_INTERRUPT lets next completion raise ISR",
              VIRTIO_SPEC_V1_2, "2.7.7");
