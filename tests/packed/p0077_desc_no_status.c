/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0077: desc_no_status
 *
 * Packed twin of T0013. A blk request with a header and a data
 * descriptor but no trailing status descriptor. The device writes the
 * completion status into a mandatory one byte writable descriptor; a
 * device that assumes it is present may read past the chain or fault.
 */
#include "tests/packed/packed_util.h"

static test_result_t test_packed_desc_no_status(struct virtio_dev *dev,
                                               struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;

    uint16_t a0 = vr->next_avail;
    uint8_t w0 = vr->wrap_counter;

    pk_append(vr, vv_virt_to_phys(hdr), sizeof(*hdr), a0,
              VRING_PACKED_DESC_F_NEXT);
    /* Chain ends here with no status descriptor. */
    pk_append(vr, vv_virt_to_phys(data), 512, a0,
              VRING_PACKED_DESC_F_WRITE);

    return vv_kick_and_wait_packed(dev, vr, 0, a0, w0, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0077, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_desc_no_status,
                     "Packed blk chain with no status descriptor",
                     VIRTIO_SPEC_V1_2, "2.8.6");
