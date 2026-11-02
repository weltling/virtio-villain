/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0146: zero length data segment in the middle of a chain
 *
 * Spec 2.7.5.2 lets a chain include a descriptor with len 0. For
 * a block read the data descriptor must carry the sector data, so
 * splitting the writable buffer into a zero length first segment
 * followed by a 512 byte segment is malformed. The device must
 * reject the request or return non zero status, but it must not
 * wedge or write past the end of the empty buffer.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_zero_data_seg(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *empty = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    *st = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, empty, 0, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 3);
    vv_desc(vr, 3, st, 1, VRING_DESC_F_WRITE, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);

    /* If the device completed it then status should still be sane */
    if (r == TEST_PASS && *st == 0xFF)
        TFAIL("r == TEST_PASS && *st == 0xFF");

    return r;
}

REGISTER_TEST(B0146, VIRTIO_PCI_DEVICE_BLK, test_blk_zero_data_seg,
              "zero length writable segment before the real data buffer",
              VIRTIO_SPEC_V1_2, "2.7.5.2");
