/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0165: FLUSH with a non zero ioprio field in the header.
 *
 * Spec 5.2.6: The ioprio field of virtio_blk_outhdr is advisory
 * for non I/O ops. Submit a FLUSH with ioprio=0xFFFFFFFF and the
 * device must ignore the field and complete the flush normally,
 * not treat it as a request length or sector hint.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_flush_high_ioprio(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_FLUSH, 0);
    uint8_t *status = vv_alloc_pages(1);

    hdr->ioprio = 0xFFFFFFFFu;
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0165, VIRTIO_PCI_DEVICE_BLK,
              test_blk_flush_high_ioprio,
              "FLUSH with non zero ioprio in the header",
              VIRTIO_SPEC_V1_2, "5.2.6");
