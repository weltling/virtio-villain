/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0078: Read with exactly 1-byte data descriptor
 *
 * Spec 5.2.6.1: "The length of data MUST be a multiple of 512 bytes
 * for VIRTIO_BLK_T_IN and VIRTIO_BLK_T_OUT requests."
 *
 * Submit a read with data length = 1 byte (not 512 multiple).
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_read_one_byte(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    /* 1 byte — not a sector multiple */
    vv_desc(vr, 1, data, 1, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0078, VIRTIO_PCI_DEVICE_BLK, test_blk_read_one_byte,
              "Read request with 1-byte data (not 512 multiple)",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
