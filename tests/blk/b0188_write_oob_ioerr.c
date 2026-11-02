/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0188: write beyond capacity returns S_IOERR.
 *
 * Spec 5.2.6.2: A driver MUST NOT submit a request which would cause
 * a write beyond capacity. Submit a write at sector = capacity.
 * The device must return S_IOERR.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_write_oob(struct virtio_dev *dev,
                                        struct vring *vr)
{
    if (!dev->device_cfg || dev->device_cfg_length < 8)
        return TEST_SKIP;

    volatile struct virtio_blk_config_head *bcfg =
        (volatile struct virtio_blk_config_head *)dev->device_cfg;
    uint64_t cap = bcfg->capacity;
    if (cap == 0) return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_OUT, cap);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    memset(data, 0xAB, 512);
    *st = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r != TEST_PASS) return r;

    if (*st != VIRTIO_BLK_S_IOERR)
        TFAIL("status %u for OOB write, expected S_IOERR (1)", *st);

    return TEST_PASS;
}

REGISTER_TEST(B0188, VIRTIO_PCI_DEVICE_BLK, test_blk_write_oob,
              "Write at capacity returns S_IOERR",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
