/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0099: Write with odd-sized data (not multiple of 512)
 *
 * Spec 5.2.6.1: data length should be a multiple of 512 for
 * read/write. Submit a WRITE with 513 bytes of data.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_write_513_bytes(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_OUT, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    memset(data, 0xCC, 513);
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 513, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0099, VIRTIO_PCI_DEVICE_BLK, test_blk_write_513_bytes,
              "Write with 513 bytes of data (not multiple of 512)",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
