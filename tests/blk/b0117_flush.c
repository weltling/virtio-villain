/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0117: blk_flush
 *
 * Submit a VIRTIO_BLK_T_FLUSH request to ensure data durability.
 * Spec 5.2.6.1: If VIRTIO_BLK_F_FLUSH is negotiated, the device
 * MUST complete the flush before reporting used.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_flush(struct virtio_dev *dev,
                                    struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_FLUSH, 0);
    uint8_t *status = vv_alloc_pages(1);

    hdr->sector = 0;  /* sector is ignored for flush */

    *status = 0xFF;

    /* Flush: header (device-readable) + status (device-writable) */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0117, VIRTIO_PCI_DEVICE_BLK, test_blk_flush,
              "Flush (write barrier) request",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
