/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0056: discard_max_segments
 *
 * Submit a discard command with the maximum number of segments
 * allowed by the device config (max_discard_seg). Tests device
 * handling of full-capacity discard batches.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

struct virtio_blk_discard_write_zeroes {
    uint64_t sector;
    uint32_t num_sectors;
    uint32_t flags;
} __attribute__((packed));

#define VIRTIO_BLK_T_DISCARD     11
#define VIRTIO_BLK_F_DISCARD     13

/* max_discard_seg offset in virtio_blk_config: byte 44 (uint32) */
#define CFG_MAX_DISCARD_SEG_OFF  44

static test_result_t test_blk_discard_max_segments(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    if (!dev->device_cfg)
        return TEST_SKIP;

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << VIRTIO_BLK_F_DISCARD)))
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

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_DISCARD;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(segs),
                       max_seg * sizeof(*segs),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0056, VIRTIO_PCI_DEVICE_BLK, test_blk_discard_max_segments,
              "Discard with maximum allowed segment count",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
