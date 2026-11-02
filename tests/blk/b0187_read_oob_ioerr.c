/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0187: read beyond capacity returns IOERR status.
 *
 * Spec 5.2.6.2: A driver MUST NOT submit a request which would cause
 * a read beyond capacity. If the device detects this, it should
 * return S_IOERR. Submit a read at sector = capacity (one past end).
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_read_oob_status(struct virtio_dev *dev,
                                              struct vring *vr)
{
    if (!dev->device_cfg || dev->device_cfg_length < 8)
        return TEST_SKIP;

    volatile struct virtio_blk_config_head *bcfg =
        (volatile struct virtio_blk_config_head *)dev->device_cfg;
    uint64_t cap = bcfg->capacity;
    if (cap == 0) return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->sector = cap;  /* one past the last valid sector */
    *st = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r != TEST_PASS) return r;

    if (*st != VIRTIO_BLK_S_IOERR)
        TFAIL("status %u for OOB read, expected S_IOERR (1)", *st);

    return TEST_PASS;
}

REGISTER_TEST(B0187, VIRTIO_PCI_DEVICE_BLK, test_blk_read_oob_status,
              "Read at capacity returns S_IOERR",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
