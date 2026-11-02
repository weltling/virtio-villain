/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0120: blk_get_id
 *
 * Submit a VIRTIO_BLK_T_GET_ID request to read the device serial number.
 * Spec 5.2.6.1: The device fills 20 bytes of serial data into the
 * device-writable buffer.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_get_id(struct virtio_dev *dev,
                                     struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_GET_ID, 0);
    uint8_t *id_buf = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    memset(id_buf, 0xFF, VIRTIO_BLK_ID_BYTES);
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, id_buf, VIRTIO_BLK_ID_BYTES,
            VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0120, VIRTIO_PCI_DEVICE_BLK, test_blk_get_id,
              "Get device identification string",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
