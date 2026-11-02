/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0068: Discard with zero-length data (empty segment list)
 *
 * Spec 5.2.6.1: data must be a multiple of 16, but 0 is technically
 * a multiple of 16 — tests that the device handles an empty discard
 * segment list gracefully (no crash or hang).
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_discard_empty(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_DISCARD, 0);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* header → status, no data descriptor at all (0 segments) */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0068, VIRTIO_PCI_DEVICE_BLK, test_blk_discard_empty,
              "Discard with no data descriptor (zero segments)",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
