/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0177: multi sector write then read verify.
 *
 * Write 4 KiB (8 sectors) of data to sectors 0..7, then read all
 * 8 sectors back in one request and verify byte by byte. Tests
 * the device handles multi-sector I/O correctly.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_multi_sector(struct virtio_dev *dev,
                                           struct vring *vr)
{
    /* Write 4K */
    struct virtio_blk_outhdr *whdr = blk_hdr(VIRTIO_BLK_T_OUT, 0);
    uint8_t *wdata = vv_alloc_pages(1);
    uint8_t *wst = vv_alloc_pages(1);

    for (int i = 0; i < 4096; i++)
        wdata[i] = (uint8_t)((i * 7 + 13) & 0xFF);
    *wst = 0xFF;

    vv_desc(vr, 0, whdr, sizeof(*whdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, wdata, 4096, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, wst, 1, VRING_DESC_F_WRITE, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r != TEST_PASS) return r;
    if (*wst != VIRTIO_BLK_S_OK) TFAIL("write status %u", *wst);

    /* Read 4K */
    struct virtio_blk_outhdr *rhdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *rdata = vv_alloc_pages(1);
    uint8_t *rst = vv_alloc_pages(1);

    *rst = 0xFF;

    vv_desc(vr, 0, rhdr, sizeof(*rhdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, rdata, 4096, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, rst, 1, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*rst != VIRTIO_BLK_S_OK) TFAIL("read status %u", *rst);

    for (int i = 0; i < 4096; i++) {
        uint8_t expected = (uint8_t)((i * 7 + 13) & 0xFF);
        if (rdata[i] != expected)
            TFAIL("byte %d: 0x%02x != 0x%02x", i, rdata[i], expected);
    }
    return TEST_PASS;
}

REGISTER_TEST(B0177, VIRTIO_PCI_DEVICE_BLK, test_blk_multi_sector,
              "Multi sector (4K) write then read verifies data",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
