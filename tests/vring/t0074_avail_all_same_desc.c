/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0074: avail_all_entries_same_desc
 *
 * Fill the available ring with entries all pointing to the same
 * descriptor index. The device must handle repeated head indices
 * without aliasing internal state or double-completing.
 */
#include "lib/virtio_test.h"

static test_result_t test_avail_all_same_desc(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    /* Post the same head (0) into 8 avail ring slots */
    for (uint16_t i = 0; i < 8; i++)
        vring_raw_set_avail(vr, i, 0);
    vring_raw_set_avail_idx(vr, 8);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0074, VIRTIO_PCI_DEVICE_BLK, test_avail_all_same_desc,
              "All avail ring entries point to same descriptor",
              VIRTIO_SPEC_V1_2, "2.7.5");
