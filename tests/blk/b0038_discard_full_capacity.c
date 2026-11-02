/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0038: blk_discard_full_capacity
 *
 * Submit a DISCARD covering the entire device capacity in one segment.
 * Tests whether the VMM can handle a discard spanning the full disk
 * without integer overflow in sector arithmetic.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_discard_full(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_DISCARD, 0);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* Discard the entire 16 MiB disk = 32768 sectors */
    seg->sector = 0;
    seg->num_sectors = 32768;
    seg->flags = 0;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, seg, sizeof(*seg), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0038, VIRTIO_PCI_DEVICE_BLK, test_blk_discard_full,
              "DISCARD covering entire device capacity",
              VIRTIO_SPEC_V1_2, "5.2.6.4");
