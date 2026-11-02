/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0067: desc_write_and_indirect_flags
 *
 * Set both WRITE and INDIRECT flags on a non-first descriptor in a
 * chain. The spec says INDIRECT must only appear on the first
 * descriptor and cannot be combined with other chain semantics.
 */
#include "lib/virtio_test.h"

static test_result_t test_desc_write_indirect(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* First descriptor: normal header */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);

    /* Second descriptor: WRITE | INDIRECT (illegal combination) */
    vv_desc(vr, 1, data, 512,
            VRING_DESC_F_NEXT | VRING_DESC_F_WRITE | VRING_DESC_F_INDIRECT, 2);

    /* Third descriptor: normal status */
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0067, VIRTIO_PCI_DEVICE_BLK, test_desc_write_indirect,
              "WRITE|INDIRECT flags on non-first descriptor",
              VIRTIO_SPEC_V1_2, "2.7.5");
