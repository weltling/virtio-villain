/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0170: write_zeroes_then_read
 *
 * Submit a WRITE_ZEROES over sectors 0..7, wait for completion,
 * then READ sector 0. Spec 5.2.6.2 says "After a write zeroes
 * command is completed, reads of the specified ranges of sectors
 * MUST return zeroes." Verify the read data is all zeros.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_wz_then_read(struct virtio_dev *dev,
                                           struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_BLK_F_WRITE_ZEROES))
        return TEST_SKIP;

    /* First: write non-zero data to sector 0 so we know zeroes are fresh */
    struct virtio_blk_outhdr *whdr = blk_hdr(VIRTIO_BLK_T_OUT, 0);
    uint8_t *wdata = vv_alloc_pages(1);
    uint8_t *wst = vv_alloc_pages(1);

    memset(wdata, 0xAA, 512);
    *wst = 0xFF;

    vv_desc(vr, 0, whdr, sizeof(*whdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, wdata, 512, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, wst, 1, VRING_DESC_F_WRITE, 0);

    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r != TEST_PASS)
        return r;
    if (*wst != VIRTIO_BLK_S_OK)
        TFAIL("write returned status %u", *wst);

    /* Second: WRITE_ZEROES sectors 0..7 */
    struct virtio_blk_outhdr *zhdr = blk_hdr(VIRTIO_BLK_T_WRITE_ZEROES, 0);
    struct virtio_blk_discard_write_zeroes *zseg = vv_alloc_pages(1);
    uint8_t *zst = vv_alloc_pages(1);

    zseg->sector = 0;
    zseg->num_sectors = 8;
    zseg->flags = 0;
    *zst = 0xFF;

    vv_desc(vr, 0, zhdr, sizeof(*zhdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, zseg, sizeof(*zseg), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, zst, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    if (*zst != VIRTIO_BLK_S_OK)
        TFAIL("write_zeroes returned status %u", *zst);

    /* Third: READ sector 0, verify all zeros */
    struct virtio_blk_outhdr *rhdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *rdata = vv_alloc_pages(1);
    uint8_t *rst = vv_alloc_pages(1);

    memset(rdata, 0xBB, 512);
    *rst = 0xFF;

    vv_desc(vr, 0, rhdr, sizeof(*rhdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, rdata, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, rst, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 2, 0);
    vring_raw_set_avail_idx(vr, 3);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    if (*rst != VIRTIO_BLK_S_OK)
        TFAIL("read returned status %u", *rst);

    /* Verify data is all zeros */
    for (int i = 0; i < 512; i++) {
        if (rdata[i] != 0)
            TFAIL("byte %d is 0x%02x, expected 0x00", i, rdata[i]);
    }
    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0170, VIRTIO_PCI_DEVICE_BLK, test_blk_wz_then_read,
              "Write zeroes then read verifies data is all zeros",
              VIRTIO_SPEC_V1_2, "5.2.6.2",
              (1ULL << VIRTIO_BLK_F_WRITE_ZEROES), 0);
