/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0087: Write with data larger than blk_size_max (if configured)
 *
 * Spec 5.2.5.2: device may set size_max. Submit a WRITE with a
 * data buffer of 65536 bytes (64KB) which may exceed size_max
 * if the device configured a smaller limit.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_write_exceeds_size_max(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_OUT, 0);
    /* 64KB = 16 pages */
    uint8_t *data = vv_alloc_pages(16);
    uint8_t *status = vv_alloc_pages(1);

    memset(data, 0x77, 65536);
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 65536, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0087, VIRTIO_PCI_DEVICE_BLK, test_blk_write_exceeds_size_max,
              "Write with 64KB data (may exceed blk_size_max)",
              VIRTIO_SPEC_V1_2, "5.2.5");
