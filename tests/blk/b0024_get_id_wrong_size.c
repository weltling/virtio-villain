/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0024: blk_get_id_wrong_size
 *
 * Send GET_ID request with data buffer != 20 bytes (the mandated
 * VIRTIO_BLK_ID_BYTES). Use a 1-byte buffer instead.
 * Spec 5.2.6.1: device uses 20 bytes for serial number.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_get_id_wrong_size(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_GET_ID, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* Chain: header -> 1-byte data buffer (should be 20) -> status */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 1, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0024, VIRTIO_PCI_DEVICE_BLK, test_blk_get_id_wrong_size,
              "GET_ID with data buffer != 20 bytes",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
