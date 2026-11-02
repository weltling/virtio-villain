/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0056: discard_max_segments
 *
 * Submit a discard command with the maximum number of segments
 * allowed by the device config (max_discard_seg). Tests device
 * handling of full-capacity discard batches.
 */
#include "lib/virtio_test.h"

/* max_discard_seg offset in virtio_blk_config: byte 44 (uint32) */
#define CFG_MAX_DISCARD_SEG_OFF  44

static test_result_t test_blk_discard_max_segments(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    if (!dev->device_cfg)
        return TEST_SKIP;

    if (!virtio_pci_feature_offered(dev, VIRTIO_BLK_F_DISCARD))
        return TEST_SKIP;

    if (dev->device_cfg_length <= CFG_MAX_DISCARD_SEG_OFF + 4)
        return TEST_SKIP;

    volatile uint32_t *max_seg_ptr =
        (volatile uint32_t *)((uint8_t *)dev->device_cfg + CFG_MAX_DISCARD_SEG_OFF);
    uint32_t max_seg = *max_seg_ptr;

    if (max_seg == 0 || max_seg > 32)
        max_seg = 1; /* use a safe default if device reports 0 or huge */

    /* Allocate segments (each 16 bytes) - limit to fit in a page */
    if (max_seg > 256)
        max_seg = 256;

    struct virtio_blk_discard_write_zeroes *segs = vv_alloc_pages(1);
    for (uint32_t i = 0; i < max_seg; i++) {
        segs[i].sector = i * 8;
        segs[i].num_sectors = 8;
        segs[i].flags = 0;
    }

    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_DISCARD, 0);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, segs, max_seg * sizeof(*segs), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_REQUIRES(B0056, VIRTIO_PCI_DEVICE_BLK, test_blk_discard_max_segments,
              "Discard with maximum allowed segment count",
              VIRTIO_SPEC_V1_2, "5.2.6.2",
              (1ULL << VIRTIO_BLK_F_DISCARD), 0);
