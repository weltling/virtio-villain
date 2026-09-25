/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0079: desc_status_not_writable
 *
 * Packed twin of T0012. The status descriptor of a blk request lacks
 * the WRITE flag. The device writes the completion status byte there,
 * so a device readable status descriptor is invalid. The device must
 * reject it instead of writing into a read only buffer.
 */
#include "tests/packed/packed_util.h"

static test_result_t test_packed_desc_status_not_writable(struct virtio_dev *dev,
                                                         struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint16_t a0 = vr->next_avail;
    uint8_t w0 = vr->wrap_counter;

    pk_append(vr, vv_virt_to_phys(hdr), sizeof(*hdr), a0,
              VRING_PACKED_DESC_F_NEXT);
    pk_append(vr, vv_virt_to_phys(data), 512, a0,
              VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE);
    /* Status descriptor without the WRITE flag. */
    pk_append(vr, vv_virt_to_phys(status), 1, a0, 0);

    return vv_kick_and_wait_packed(dev, vr, 0, a0, w0, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0079, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_desc_status_not_writable,
                     "Packed status descriptor without the WRITE flag",
                     VIRTIO_SPEC_V1_2, "2.8.6");
