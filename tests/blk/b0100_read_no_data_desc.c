/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0100: Request with only header and status (no data for READ)
 *
 * Spec 5.2.6: A READ request should have a data segment.
 * Submit header + status only, no data descriptor in between.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_read_no_data_desc(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* Only header -> status, no data descriptor */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0100, VIRTIO_PCI_DEVICE_BLK, test_blk_read_no_data_desc,
              "READ request with no data descriptor (header + status only)",
              VIRTIO_SPEC_V1_2, "5.2.6");
