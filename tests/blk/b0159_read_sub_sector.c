/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0159: blk_read_sub_sector
 *
 * Submit a read at sector 0 with a data buffer length of 200
 * bytes, well below the 512 byte sector size. Spec 5.2.6 says
 * data length must be a multiple of 512. The device must reject
 * or stay silent without partial completion or crash.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_read_sub_sector(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 200, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0159, VIRTIO_PCI_DEVICE_BLK, test_blk_read_sub_sector,
              "READ at sector 0 with sub sector data length",
              VIRTIO_SPEC_V1_2, "5.2.6");
