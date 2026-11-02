/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0111: used ring len field matches bytes written for blk read.
 *
 * Spec 2.7.8: The device writes the number of bytes it has written
 * to memory into the used ring entry's len field. For a blk READ
 * of 512 bytes, the used len must be 512 + 1 (data + status byte).
 */
#include "lib/virtio_test.h"

static test_result_t test_used_len_verify(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    *st = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r != TEST_PASS) return r;

    /* Check used ring entry len */
    uint32_t used_len = vr->used->ring[0].len;

    /* Expected: 512 (data) + 1 (status) = 513 */
    if (used_len != 513)
        TFAIL("used len %u, expected 513 (512 data + 1 status)",
              used_len);

    return TEST_PASS;
}

REGISTER_TEST(T0111, VIRTIO_PCI_DEVICE_BLK, test_used_len_verify,
              "Used ring len matches bytes written for blk read",
              VIRTIO_SPEC_V1_2, "2.7.8");
