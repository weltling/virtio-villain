/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Shared helpers for packed ring descriptor tests. Builds packed blk
 * request chains with the correct AVAIL/USED phase bits so individual
 * tests only vary the interesting field.
 */
#ifndef VV_PACKED_UTIL_H
#define VV_PACKED_UTIL_H

#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

/*
 * Append one descriptor at the next available slot, stamping the
 * AVAIL/USED phase bits for the current wrap counter, then advance.
 * The caller supplies only the non phase flags (NEXT, WRITE, INDIRECT).
 */
static inline void pk_append(struct vring_packed *vr, uint64_t addr,
                             uint32_t len, uint16_t id, uint16_t flags)
{
    uint16_t i = vr->next_avail;
    uint16_t av = vr->wrap_counter ? VRING_PACKED_DESC_F_AVAIL : 0;
    uint16_t us = vr->wrap_counter ? 0 : VRING_PACKED_DESC_F_USED;

    vr->desc[i].addr = addr;
    vr->desc[i].len = len;
    vr->desc[i].id = id;
    __sync_synchronize();
    vr->desc[i].flags = (uint16_t)(flags | av | us);
    vring_packed_advance(vr);
}

/*
 * Build and submit a three descriptor blk request: header (readable),
 * data, status (writable). The data descriptor uses the caller
 * supplied address, length, and extra flags, so a test can point it
 * anywhere or drop the WRITE bit to reverse the DMA direction. Returns
 * the completion verdict.
 */
static inline test_result_t pk_blk_chain(struct virtio_dev *dev,
                                         struct vring_packed *vr,
                                         uint32_t hdr_type,
                                         uint64_t data_addr,
                                         uint32_t data_len,
                                         uint16_t data_flags)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = hdr_type;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint16_t a0 = vr->next_avail;
    uint8_t w0 = vr->wrap_counter;

    pk_append(vr, vv_virt_to_phys(hdr), sizeof(*hdr), a0,
              VRING_PACKED_DESC_F_NEXT);
    pk_append(vr, data_addr, data_len, a0,
              (uint16_t)(VRING_PACKED_DESC_F_NEXT | data_flags));
    pk_append(vr, vv_virt_to_phys(status), 1, a0,
              VRING_PACKED_DESC_F_WRITE);

    return vv_kick_and_wait_packed(dev, vr, 0, a0, w0, VV_TIMEOUT_MS);
}

/*
 * Fill a fresh page with a valid three entry blk read chain in packed
 * indirect descriptor layout and return its physical address. The
 * entries are chained by the NEXT flag and carry no phase bits, as
 * required for indirect tables.
 */
static inline uint64_t pk_indirect_blk_table(void)
{
    struct vring_packed_desc *tab = vv_alloc_pages(1);
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    tab[0].addr = vv_virt_to_phys(hdr);
    tab[0].len = sizeof(*hdr);
    tab[0].id = 0;
    tab[0].flags = VRING_PACKED_DESC_F_NEXT;
    tab[1].addr = vv_virt_to_phys(data);
    tab[1].len = 512;
    tab[1].id = 1;
    tab[1].flags = VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE;
    tab[2].addr = vv_virt_to_phys(status);
    tab[2].len = 1;
    tab[2].id = 2;
    tab[2].flags = VRING_PACKED_DESC_F_WRITE;

    return vv_virt_to_phys(tab);
}

#endif /* VV_PACKED_UTIL_H */
