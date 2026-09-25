/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0073: cross_format_indirect
 *
 * Packed twin of T0130. A packed indirect descriptor points at a table
 * laid out in split descriptor format (addr, len, flags, next) instead
 * of packed format (addr, len, id, flags). A device reading it as
 * packed misreads the id and flags fields. It must reject or safely
 * handle the malformed table rather than follow garbage flags.
 */
#include "tests/packed/packed_util.h"

static test_result_t test_packed_cross_format_indirect(struct virtio_dev *dev,
                                                      struct vring_packed *vr)
{
    struct vring_desc *tab = vv_alloc_pages(1);
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    /* Build a valid blk chain, but in split descriptor layout. */
    tab[0].addr = vv_virt_to_phys(hdr);
    tab[0].len = sizeof(*hdr);
    tab[0].flags = VRING_DESC_F_NEXT;
    tab[0].next = 1;
    tab[1].addr = vv_virt_to_phys(data);
    tab[1].len = 512;
    tab[1].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
    tab[1].next = 2;
    tab[2].addr = vv_virt_to_phys(status);
    tab[2].len = 1;
    tab[2].flags = VRING_DESC_F_WRITE;
    tab[2].next = 0;

    uint16_t a0 = vr->next_avail;
    uint8_t w0 = vr->wrap_counter;

    pk_append(vr, vv_virt_to_phys(tab), 3 * sizeof(struct vring_desc), a0,
              VRING_PACKED_DESC_F_INDIRECT);

    return vv_kick_and_wait_packed(dev, vr, 0, a0, w0, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0073, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_cross_format_indirect,
                     "Packed indirect table laid out in split format",
                     VIRTIO_SPEC_V1_2, "2.8.6");
