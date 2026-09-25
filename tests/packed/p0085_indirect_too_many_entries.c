/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0085: indirect_too_many_entries
 *
 * A packed indirect descriptor whose table declares more entries than
 * the queue size. The spec caps an indirect table at the queue size.
 * The device must reject the oversized table rather than walking past
 * its own limit.
 */
#include "tests/packed/packed_util.h"

static test_result_t test_packed_indirect_too_many_entries(struct virtio_dev *dev,
                                                          struct vring_packed *vr)
{
    uint32_t n = (uint32_t)vr->size + 1;
    uint32_t bytes = n * (uint32_t)sizeof(struct vring_packed_desc);
    uint32_t pages = (bytes + 4095) / 4096;

    struct vring_packed_desc *tab = vv_alloc_pages(pages);
    uint8_t *scratch = vv_alloc_pages(1);
    uint64_t sp = vv_virt_to_phys(scratch);

    for (uint32_t i = 0; i < n; i++) {
        tab[i].addr = sp;
        tab[i].len = 16;
        tab[i].id = (uint16_t)i;
        tab[i].flags = (i + 1 < n) ? VRING_PACKED_DESC_F_NEXT
                                   : VRING_PACKED_DESC_F_WRITE;
    }

    uint16_t a0 = vr->next_avail;
    uint8_t w0 = vr->wrap_counter;

    pk_append(vr, vv_virt_to_phys(tab), bytes, a0,
              VRING_PACKED_DESC_F_INDIRECT);

    return vv_kick_and_wait_packed(dev, vr, 0, a0, w0, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0085, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_indirect_too_many_entries,
                     "Packed indirect table with more entries than queue size",
                     VIRTIO_SPEC_V1_2, "2.8.6");
