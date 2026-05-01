/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0017: blk_wz_unknown_flags
 *
 * Submit a WRITE ZEROES request with reserved flag bits set.
 * The device should return UNSUPP for unknown flags.
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

#define VIRTIO_BLK_T_WRITE_ZEROES 14

static test_result_t test_blk_wz_unknown_flags(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_WRITE_ZEROES;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    seg->sector = 0;
    seg->num_sectors = 8;
    seg->flags = 0xFFFFFFFE; /* reserved bits set */

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t seg_phys = vv_virt_to_phys(seg);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, seg_phys, sizeof(*seg),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0017, VIRTIO_PCI_DEVICE_BLK, test_blk_wz_unknown_flags,
              "WRITE ZEROES with reserved flag bits set",
              VIRTIO_SPEC_V1_2, "5.2.6");
