/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0097: Status descriptor with length 0 (must be at least 1 byte)
 *
 * Spec 5.2.6: The final status byte is device-writable, 1 byte.
 * Provide a status descriptor with length 0.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_status_len_zero(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    /* Status descriptor with length 0 */
    vv_desc(vr, 2, status, 0, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0097, VIRTIO_PCI_DEVICE_BLK, test_blk_status_len_zero,
              "Status descriptor with length 0",
              VIRTIO_SPEC_V1_2, "5.2.6");
