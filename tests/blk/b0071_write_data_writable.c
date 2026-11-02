/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0071: Write request with data descriptor marked writable
 *
 * For VIRTIO_BLK_T_OUT (write), the data must be device-readable.
 * Submit a write where the data descriptor has VRING_DESC_F_WRITE
 * (device-writable), so the device cannot read the write payload.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_write_data_writable(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_OUT, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    memset(data, 0x55, 512);
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    /* Data marked WRITE — wrong for a write request */
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0071, VIRTIO_PCI_DEVICE_BLK, test_blk_write_data_writable,
              "Write request with data descriptor marked device-writable",
              VIRTIO_SPEC_V1_2, "5.2.6");
