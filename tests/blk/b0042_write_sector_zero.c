/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0042: blk_write_sector_zero
 *
 * Write to sector 0 (the boot sector). Some VMM backends special-case
 * sector 0 for partition table handling or copy-on-write. This tests
 * whether writing to sector 0 triggers any assertion failures or
 * different code paths compared to other sectors.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_write_sector_zero(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_OUT, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->sector = 0; /* boot sector */
    *status = 0xFF;

    /* Fill with a recognizable pattern */
    memset(data, 0xAA, 512);

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0042, VIRTIO_PCI_DEVICE_BLK, test_blk_write_sector_zero,
              "Write to sector 0 (boot sector)",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
