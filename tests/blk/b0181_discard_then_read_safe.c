/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0181: discard then read does not crash.
 *
 * Spec 5.2.6.2: After DISCARD the content of the affected range
 * is undefined, but the device must not crash or return stale
 * data from a freed backing page. Submit DISCARD on sectors 8..15,
 * then READ sector 8 and accept any data without asserting content.
 * The value is that the device survives the sequence.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_discard_then_read_safe(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_BLK_F_DISCARD))
        return TEST_SKIP;

    /* DISCARD sectors 8..15 */
    struct virtio_blk_outhdr *dhdr = blk_hdr(VIRTIO_BLK_T_DISCARD, 0);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *dst = vv_alloc_pages(1);

    seg->sector = 8;
    seg->num_sectors = 8;
    seg->flags = 0;
    *dst = 0xFF;

    vv_desc(vr, 0, dhdr, sizeof(*dhdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, seg, sizeof(*seg), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, dst, 1, VRING_DESC_F_WRITE, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r != TEST_PASS) return r;
    if (*dst != VIRTIO_BLK_S_OK) TFAIL("discard status %u", *dst);

    /* READ sector 8, accept any content */
    struct virtio_blk_outhdr *rhdr = blk_hdr(VIRTIO_BLK_T_IN, 8);
    uint8_t *rdata = vv_alloc_pages(1);
    uint8_t *rst = vv_alloc_pages(1);

    *rst = 0xFF;

    vv_desc(vr, 0, rhdr, sizeof(*rhdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, rdata, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, rst, 1, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*rst != VIRTIO_BLK_S_OK) TFAIL("read status %u", *rst);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0181, VIRTIO_PCI_DEVICE_BLK,
              test_blk_discard_then_read_safe,
              "Discard then read the same range survives",
              VIRTIO_SPEC_V1_2, "5.2.6.2",
              (1ULL << VIRTIO_BLK_F_DISCARD), 0);
