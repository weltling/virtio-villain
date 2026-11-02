/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0096: Header descriptor length larger than struct (extra bytes)
 *
 * Spec 5.2.6: The header is exactly 16 bytes. Provide a header
 * descriptor with length 32, containing extra garbage bytes.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_header_oversized(struct virtio_dev *dev,
                                               struct vring *vr)
{
    uint8_t *hdr_buf = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    struct virtio_blk_outhdr *hdr = (struct virtio_blk_outhdr *)hdr_buf;
    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    /* Fill extra bytes with garbage */
    memset(hdr_buf + 16, 0xDE, 16);
    *status = 0xFF;

    /* Header descriptor is 32 bytes instead of 16 */
    vv_desc(vr, 0, hdr_buf, 32, VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0096, VIRTIO_PCI_DEVICE_BLK, test_blk_header_oversized,
              "Header descriptor with length 32 (oversized by 16 bytes)",
              VIRTIO_SPEC_V1_2, "5.2.6");
