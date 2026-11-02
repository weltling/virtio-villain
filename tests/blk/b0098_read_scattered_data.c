/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0098: Read request with two data descriptors (scattered read)
 *
 * Submit a READ with data split across two 256-byte writable
 * descriptors (total 512 bytes). Tests scatter-gather handling.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_read_scattered_data(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data1 = vv_alloc_pages(1);
    uint8_t *data2 = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data1, 256, VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, data2, 256, VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 3);
    vv_desc(vr, 3, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0098, VIRTIO_PCI_DEVICE_BLK, test_blk_read_scattered_data,
              "Read with data split across two 256-byte descriptors",
              VIRTIO_SPEC_V1_2, "5.2.6");
