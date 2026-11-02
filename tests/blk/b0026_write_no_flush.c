/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0026: blk_write_no_flush
 *
 * Issue a WRITE request and immediately read back without an
 * intervening FLUSH. Tests whether the device assumes writes are
 * durable without explicit flush. This is a correctness probe -
 * we verify the device processes both requests without crash.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_write_no_flush(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_blk_outhdr *hdr_w = blk_hdr(VIRTIO_BLK_T_OUT, 0);
    struct virtio_blk_outhdr *hdr_r = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *wdata = vv_alloc_pages(1);
    uint8_t *rdata = vv_alloc_pages(1);
    uint8_t *status_w = vv_alloc_pages(1);
    uint8_t *status_r = vv_alloc_pages(1);

    /* Write request: sector 0 */
    memset(wdata, 0x42, 512);
    *status_w = 0xFF;

    /* Read request: sector 0 (no flush in between) */
    *status_r = 0xFF;

    /* Request 1 (write): descs 0-2 */
    vv_desc(vr, 0, hdr_w, sizeof(*hdr_w), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, wdata, 512, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status_w, 1, VRING_DESC_F_WRITE, 0);

    /* Request 2 (read): descs 3-5 */
    vv_desc(vr, 3, hdr_r, sizeof(*hdr_r), VRING_DESC_F_NEXT, 4);
    vv_desc(vr, 4, rdata, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 5);
    vv_desc(vr, 5, status_r, 1, VRING_DESC_F_WRITE, 0);

    /* Both in avail ring */
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 3);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0026, VIRTIO_PCI_DEVICE_BLK, test_blk_write_no_flush,
              "Write then read without intervening flush",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
