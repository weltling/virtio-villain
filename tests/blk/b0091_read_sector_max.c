/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0091: Read with sector == UINT64_MAX (maximum possible sector)
 *
 * Submit a READ with sector set to the maximum 64-bit value.
 * The device should reject this as beyond capacity.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_read_sector_max(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, UINT64_MAX);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0091, VIRTIO_PCI_DEVICE_BLK, test_blk_read_sector_max,
              "Read with sector == UINT64_MAX",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
