/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0043: blk_discard_multiple_segments
 *
 * Submit a DISCARD request with exactly the maximum allowed number
 * of segments (typically limited by the data descriptor length
 * divided by 16 bytes per segment). This is a boundary test for
 * the segment count loop in the discard handler.
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

#define VIRTIO_BLK_T_DISCARD 11

static test_result_t test_blk_discard_multi_segments(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_DISCARD;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    /*
     * Allocate space for 4 segments (64 bytes). Each segment discards
     * a different range. All are valid (within capacity for a 16MiB disk
     * = 32768 sectors).
     */
    struct virtio_blk_discard_write_zeroes *segs = vv_alloc_pages(1);
    segs[0].sector = 0;
    segs[0].num_sectors = 8;
    segs[0].flags = 0;
    segs[1].sector = 8;
    segs[1].num_sectors = 8;
    segs[1].flags = 0;
    segs[2].sector = 16;
    segs[2].num_sectors = 8;
    segs[2].flags = 0;
    segs[3].sector = 24;
    segs[3].num_sectors = 8;
    segs[3].flags = 0;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t segs_phys = vv_virt_to_phys(segs);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, segs_phys, 4 * sizeof(*segs),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0043, VIRTIO_PCI_DEVICE_BLK, test_blk_discard_multi_segments,
              "DISCARD with 4 valid segments (boundary count)",
              VIRTIO_SPEC_V1_2, "5.2.6.4");
