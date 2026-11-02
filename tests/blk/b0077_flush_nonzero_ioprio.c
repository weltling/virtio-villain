/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0077: Flush with nonzero ioprio field
 *
 * The reserved/ioprio field should be zero. Submit a FLUSH request
 * with ioprio set to 0xFFFFFFFF to test that the device doesn't
 * misinterpret the reserved field.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_flush_nonzero_ioprio(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_FLUSH, 0);
    uint8_t *status = vv_alloc_pages(1);

    hdr->ioprio = 0xFFFFFFFF; /* reserved, should be 0 */
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0077, VIRTIO_PCI_DEVICE_BLK, test_blk_flush_nonzero_ioprio,
              "Flush request with ioprio=0xFFFFFFFF (reserved field nonzero)",
              VIRTIO_SPEC_V1_2, "5.2.6");
