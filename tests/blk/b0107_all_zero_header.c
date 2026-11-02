/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0107: All-zero header (type=0, ioprio=0, sector=0 is a valid READ at 0)
 *
 * Sanity check: a completely zeroed header is type=IN, sector 0.
 * This should succeed as a normal read of sector 0.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_all_zero_header(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0107, VIRTIO_PCI_DEVICE_BLK, test_blk_all_zero_header,
              "All-zero header (valid READ sector 0)",
              VIRTIO_SPEC_V1_2, "5.2.6");
