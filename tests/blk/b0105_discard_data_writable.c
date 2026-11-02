/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0105: Discard segment data marked as writable
 *
 * Spec 5.2.6: Discard/write-zeroes segment data is device-readable
 * (driver provides it). Mark the segment descriptor as WRITE (writable).
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_discard_data_writable(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_DISCARD, 0);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    seg->sector = 0;
    seg->num_sectors = 8;
    seg->flags = 0;

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    /* Segment marked WRITE (wrong - should be readable) */
    vv_desc(vr, 1, seg, sizeof(*seg), VRING_DESC_F_WRITE | VRING_DESC_F_NEXT,
            2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0105, VIRTIO_PCI_DEVICE_BLK, test_blk_discard_data_writable,
              "Discard with segment descriptor marked writable (wrong direction)",
              VIRTIO_SPEC_V1_2, "5.2.6");
