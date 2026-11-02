/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0156: blk_flush_nonzero_ioprio
 *
 * Spec 5.2.6.2 says ioprio is unused by the device for FLUSH.
 * Submit a FLUSH whose request header carries a non zero ioprio
 * value. The device must still complete the flush successfully.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_flush_nonzero_ioprio(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_FLUSH, 0);
    uint8_t *status = vv_alloc_pages(1);

    hdr->ioprio = 0xCAFEBABE;
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0156, VIRTIO_PCI_DEVICE_BLK, test_blk_flush_nonzero_ioprio,
              "FLUSH with non zero ioprio still completes",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
