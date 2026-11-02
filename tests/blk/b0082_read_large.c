/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0082: Read request with data spanning multiple pages (large I/O)
 *
 * Test that the device handles a large read correctly when the
 * data buffer is 64 sectors (32KB) — typical large block I/O.
 * While not a spec violation, large I/Os stress different code
 * paths in VMM backend buffer handling.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_read_large(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    /* 32KB = 8 pages */
    uint8_t *data = vv_alloc_pages(8);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    /* 32KB read (64 sectors) */
    vv_desc(vr, 1, data, 32768, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0082, VIRTIO_PCI_DEVICE_BLK, test_blk_read_large,
              "Read 32KB (64 sectors) in a single request",
              VIRTIO_SPEC_V1_2, "5.2.6");
