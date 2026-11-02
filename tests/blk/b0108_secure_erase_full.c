/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0108: Secure erase at sector 0 with num_sectors covering full device
 *
 * Submit SECURE_ERASE with sector=0 and num_sectors large enough
 * to cover far beyond any reasonable device capacity.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_secure_erase_full(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_SECURE_ERASE, 0);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    seg->sector = 0;
    seg->num_sectors = UINT32_MAX;
    seg->flags = 0;

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, seg, sizeof(*seg), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0108, VIRTIO_PCI_DEVICE_BLK, test_blk_secure_erase_full,
              "Secure erase with num_sectors == UINT32_MAX (exceeds capacity)",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
