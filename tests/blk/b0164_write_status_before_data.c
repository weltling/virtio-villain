/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0164: virtio_blk WRITE with status descriptor placed before
 * the data buffer in the chain.
 *
 * Spec 5.2.6: the chain layout for a write is header, then
 * device readable data, then a one byte device writable status.
 * Reorder the writable status descriptor so it appears between
 * the header and the data buffer. The device must reject without
 * confusing status with data or scribbling on the data buffer.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_write_status_before_data(struct virtio_dev *dev,
                                                       struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_OUT, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    memset(data, 0x5A, 512);
    *status = 0xFF;

    /* Chain: header, status (writable), data (readable). Wrong order. */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, status, 1, VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, data, 512, 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0164, VIRTIO_PCI_DEVICE_BLK,
              test_blk_write_status_before_data,
              "Write chain with status descriptor before data buffer",
              VIRTIO_SPEC_V1_2, "5.2.6");
