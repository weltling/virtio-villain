/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0001: blk_sector_beyond_capacity
 *
 * Submit a read request with sector far beyond the device capacity.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_sector_beyond_capacity(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0xFFFFFFFFFFFFULL);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    /* Sector way beyond any reasonable disk */
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0001, VIRTIO_PCI_DEVICE_BLK, test_blk_sector_beyond_capacity,
              "Read sector beyond device capacity",
              VIRTIO_SPEC_V1_2, "5.2.6");
