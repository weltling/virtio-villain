/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0109: Header marked as device-writable
 *
 * Spec 5.2.6: The header is driver-readable (device-readable).
 * Mark it with WRITE flag (device-writable) - wrong direction.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_header_device_writable(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* Header with WRITE flag (wrong - should be readable) */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_WRITE | VRING_DESC_F_NEXT,
            1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0109, VIRTIO_PCI_DEVICE_BLK, test_blk_header_device_writable,
              "Header descriptor marked device-writable (wrong direction)",
              VIRTIO_SPEC_V1_2, "5.2.6");
