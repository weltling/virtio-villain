/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0008: blk_flush_nonzero_sector
 *
 * Submit a FLUSH with sector != 0. The spec says the driver MUST set
 * sector to 0 for FLUSH.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_flush_nonzero_sector(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_FLUSH, 0);
    uint8_t *status = vv_alloc_pages(1);

    hdr->sector = 12345; /* should be 0 */
    *status = 0xFF;

    /* FLUSH: header + status only (no data) */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0008, VIRTIO_PCI_DEVICE_BLK, test_blk_flush_nonzero_sector,
              "FLUSH with sector != 0",
              VIRTIO_SPEC_V1_2, "5.2.6");
