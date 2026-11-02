/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0065: Secure erase data length not a multiple of 16
 *
 * Spec 5.2.6.1: "The length of data MUST be a multiple of the size
 * of struct virtio_blk_discard_write_zeroes" for SECURE_ERASE.
 *
 * Submit SECURE_ERASE with data length = 24 (1.5 segments).
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_secure_erase_bad_len(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_SECURE_ERASE, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    /* 24 bytes — not a multiple of 16 */
    vv_desc(vr, 1, data, 24, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0065, VIRTIO_PCI_DEVICE_BLK, test_blk_secure_erase_bad_len,
              "Secure erase data length not a multiple of 16 bytes",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
