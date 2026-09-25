/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0065: desc_addr_beyond_ram
 *
 * Packed twin of T0005. Point a packed data descriptor at a guest
 * physical address far beyond the VM's RAM. A device that translates
 * the GPA to a host pointer without a bounds check dereferences an
 * invalid pointer and faults the VMM. The device must reject the
 * address instead of accessing it.
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

static test_result_t test_packed_desc_addr_beyond_ram(struct virtio_dev *dev,
                                                     struct vring_packed *vr)
{
    return submit_blk_read_packed(dev, vr, 0xFFFFFFFFFFFF0000ULL, 512);
}

REGISTER_TEST_PACKED(P0065, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_desc_addr_beyond_ram,
                     "Packed descriptor GPA beyond guest RAM",
                     VIRTIO_SPEC_V1_2, "2.8.6");
