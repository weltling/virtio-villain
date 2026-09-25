/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0071: desc_total_len_overflow
 *
 * Packed twin of T0077. A chain of two data descriptors, each claiming
 * 2 GiB, so the summed transfer length overflows a 32 bit accumulator.
 * The individual lengths are legal but their sum exceeds 4 GiB. The
 * device must compute the total safely and not wrap or over allocate.
 */
#include "tests/packed/packed_util.h"

static test_result_t test_packed_desc_total_len_overflow(struct virtio_dev *dev,
                                                        struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t data_phys;
    vv_alloc_page_high(&data_phys);

    uint16_t a0 = vr->next_avail;
    uint8_t w0 = vr->wrap_counter;

    pk_append(vr, vv_virt_to_phys(hdr), sizeof(*hdr), a0,
              VRING_PACKED_DESC_F_NEXT);
    pk_append(vr, data_phys, 0x80000000, a0,
              VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE);
    pk_append(vr, data_phys, 0x80000000, a0,
              VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE);
    pk_append(vr, vv_virt_to_phys(status), 1, a0,
              VRING_PACKED_DESC_F_WRITE);

    return vv_kick_and_wait_packed(dev, vr, 0, a0, w0, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0071, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_desc_total_len_overflow,
                     "Packed descriptor chain total length overflows 32 bits",
                     VIRTIO_SPEC_V1_2, "2.8.6");
