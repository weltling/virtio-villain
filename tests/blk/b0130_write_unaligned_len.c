/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0130: Write with non-sector-aligned data length.
 *
 * Submit a write with a data buffer of 1025 bytes (not a multiple of
 * 512). The device must reject or handle this misalignment.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_write_unaligned_len(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_OUT, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;
    memset(data, 0xAB, 1025);

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 1025, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0130, VIRTIO_PCI_DEVICE_BLK, test_blk_write_unaligned_len,
              "Write with non-sector-aligned data length",
              VIRTIO_SPEC_V1_2, "5.2.6");
