/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0064: desc_addr_other_queue
 *
 * Packed twin of T0107. Point a device writable data descriptor on
 * queue 0 at the descriptor ring of a second packed queue. That ring
 * is device owned memory, so a blk read that DMAs disk bytes into it
 * lets the guest forge ring state on a queue it is not driving. The
 * device must not honor an address that lands on another queue's ring.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t submit_blk_read_packed(struct virtio_dev *dev,
                                            struct vring_packed *vr,
                                            uint64_t data_addr,
                                            uint32_t data_len)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint16_t a0 = vr->next_avail;
    uint8_t w0 = vr->wrap_counter;
    uint16_t av, us, i;

    i = vr->next_avail;
    av = vr->wrap_counter ? VRING_PACKED_DESC_F_AVAIL : 0;
    us = vr->wrap_counter ? 0 : VRING_PACKED_DESC_F_USED;
    vr->desc[i].addr = vv_virt_to_phys(hdr);
    vr->desc[i].len = sizeof(*hdr);
    vr->desc[i].id = a0;
    __sync_synchronize();
    vr->desc[i].flags = av | us | VRING_PACKED_DESC_F_NEXT;
    vring_packed_advance(vr);

    i = vr->next_avail;
    av = vr->wrap_counter ? VRING_PACKED_DESC_F_AVAIL : 0;
    us = vr->wrap_counter ? 0 : VRING_PACKED_DESC_F_USED;
    vr->desc[i].addr = data_addr;
    vr->desc[i].len = data_len;
    vr->desc[i].id = a0;
    __sync_synchronize();
    vr->desc[i].flags = av | us | VRING_PACKED_DESC_F_NEXT |
                        VRING_PACKED_DESC_F_WRITE;
    vring_packed_advance(vr);

    i = vr->next_avail;
    av = vr->wrap_counter ? VRING_PACKED_DESC_F_AVAIL : 0;
    us = vr->wrap_counter ? 0 : VRING_PACKED_DESC_F_USED;
    vr->desc[i].addr = vv_virt_to_phys(status);
    vr->desc[i].len = 1;
    vr->desc[i].id = a0;
    __sync_synchronize();
    vr->desc[i].flags = av | us | VRING_PACKED_DESC_F_WRITE;
    vring_packed_advance(vr);

    return vv_kick_and_wait_packed(dev, vr, 0, a0, w0, VV_TIMEOUT_MS);
}

static test_result_t test_packed_desc_addr_other_queue(struct virtio_dev *dev,
                                                      struct vring_packed *vr)
{
    if (dev->common->num_queues < 2)
        return TEST_SKIP;

    struct vring_packed vr2;
    vring_packed_alloc(&vr2, 16);
    vring_packed_attach(dev, &vr2, 1);

    return submit_blk_read_packed(dev, vr, vr2.desc_phys, 512);
}

REGISTER_TEST_PACKED(P0064, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_desc_addr_other_queue,
                     "Packed descriptor address into another queue's ring",
                     VIRTIO_SPEC_V1_2, "2.8.6");
