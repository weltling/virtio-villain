/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0089: Request with type field in high bits (unknown type)
 *
 * Submit a request with type = 0x80000000 (bit 31 set). This
 * is not a valid block request type and should be rejected.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_type_high_bit(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = 0x80000000; /* high bit, unknown type */
    hdr->sector = 0;
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0089, VIRTIO_PCI_DEVICE_BLK, test_blk_type_high_bit,
              "Request with type = 0x80000000 (unknown high-bit type)",
              VIRTIO_SPEC_V1_2, "5.2.6");
