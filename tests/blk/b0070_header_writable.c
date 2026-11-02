/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0070: Read request with header descriptor marked writable
 *
 * The virtio block request header (type, ioprio, sector) must be
 * device-readable. Submit a READ with the header descriptor
 * having VRING_DESC_F_WRITE set (device-writable).
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_hdr_writable(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* Header marked WRITE (device-writable) — wrong direction */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT | VRING_DESC_F_WRITE,
            1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0070, VIRTIO_PCI_DEVICE_BLK, test_blk_hdr_writable,
              "Read request with header descriptor marked device-writable",
              VIRTIO_SPEC_V1_2, "5.2.6");
