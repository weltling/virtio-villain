/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0121: blk_write_then_flush
 *
 * Write data then issue a flush to exercise the complete write path.
 * Spec 5.2.6.1: After flush completes, all prior writes MUST be
 * durable on the backing store.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_write_then_flush(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_OUT, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    /* First: write one sector */
    memset(data, 0xAB, 512);
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    VV_TRY(vv_submit_and_wait(dev, vr, 0));

    /* Second: flush */
    struct virtio_blk_outhdr *flush_hdr = blk_hdr(VIRTIO_BLK_T_FLUSH, 0);
    uint8_t *flush_status = vv_alloc_pages(1);

    *flush_status = 0xFF;

    vv_desc(vr, 0, flush_hdr, sizeof(*flush_hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, flush_status, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0121, VIRTIO_PCI_DEVICE_BLK, test_blk_write_then_flush,
              "Write followed by flush",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
