/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0020: packed_ring_completely_full
 *
 * Fill the entire packed ring with queue_size descriptors in-flight
 * simultaneously. Tests the device's handling of maximum ring
 * occupancy without overflow or wrap-counter confusion.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_packed_ring_full(struct virtio_dev *dev,
                                           struct vring_packed *vr)
{
    uint16_t qsz = vr->size;

    /*
     * We'll use indirect descriptors so each avail slot is one
     * descriptor (the indirect pointer), consuming all queue_size slots.
     */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    /* Build a shared indirect table for all slots */
    struct vring_desc *indirect = vv_alloc_pages(1);
    indirect[0].addr = hdr_phys;
    indirect[0].len = sizeof(*hdr);
    indirect[0].flags = VRING_DESC_F_NEXT;
    indirect[0].next = 1;
    indirect[1].addr = data_phys;
    indirect[1].len = 512;
    indirect[1].flags = VRING_DESC_F_WRITE | VRING_DESC_F_NEXT;
    indirect[1].next = 2;
    indirect[2].addr = status_phys;
    indirect[2].len = 1;
    indirect[2].flags = VRING_DESC_F_WRITE;
    indirect[2].next = 0;

    uint64_t ind_phys = vv_virt_to_phys(indirect);

    /* Fill every slot in the ring */
    for (uint16_t i = 0; i < qsz; i++) {
        uint16_t flags = VRING_PACKED_DESC_F_INDIRECT;
        /* Set avail bit matching current wrap counter */
        if (vr->wrap_counter)
            flags |= (1 << 7); /* AVAIL */
        else
            flags &= ~(1 << 7);
        /* Clear used bit (opposite of avail for "available") */
        if (vr->wrap_counter)
            flags &= ~(1 << 15);
        else
            flags |= (1 << 15);

        vr->desc[i].addr = ind_phys;
        vr->desc[i].len = 3 * sizeof(struct vring_desc);
        vr->desc[i].id = i;
        __sync_synchronize();
        vr->desc[i].flags = flags;
    }
    __sync_synchronize();

    /* Kick once for all */
    uint16_t last_idx = qsz - 1;
    return vv_kick_and_wait_packed(dev, vr, 0, last_idx,
                                   vr->wrap_counter, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0020, VIRTIO_PCI_DEVICE_BLK, test_packed_ring_full,
                     "Packed ring completely full (queue_size in-flight)",
                     VIRTIO_SPEC_V1_2, "2.8.6");
