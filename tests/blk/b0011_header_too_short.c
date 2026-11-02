/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0011: blk_header_too_short
 *
 * Submit a request where the header descriptor has len < 16 bytes
 * (sizeof virtio_blk_outhdr). The device cannot parse a truncated header.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_header_too_short(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* Header descriptor with only 4 bytes (needs 16) */
    vv_desc(vr, 0, hdr, 4, VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0011, VIRTIO_PCI_DEVICE_BLK, test_blk_header_too_short,
              "Header descriptor shorter than 16 bytes",
              VIRTIO_SPEC_V1_2, "5.2.6");
