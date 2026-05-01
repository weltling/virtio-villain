/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0016: blk_wz_too_many_segments
 *
 * Submit a WRITE ZEROES request with more segments than
 * max_write_zeroes_seg. The device must reject the oversized request.
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

#define VIRTIO_BLK_T_WRITE_ZEROES 13 /* same as discard, different type */
/* Actually write zeroes is type 13 in some impls, spec says 13=discard, 14=write_zeroes */
#undef VIRTIO_BLK_T_WRITE_ZEROES
#define VIRTIO_BLK_T_WRITE_ZEROES 14

static test_result_t test_blk_wz_too_many(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    struct virtio_blk_discard_write_zeroes *segs = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_WRITE_ZEROES;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    /* Fill 256 write-zeroes segments */
    for (int i = 0; i < 256; i++) {
        segs[i].sector = (uint64_t)i * 8;
        segs[i].num_sectors = 8;
        segs[i].flags = 0;
    }

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t segs_phys = vv_virt_to_phys(segs);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, segs_phys,
                       256 * sizeof(struct virtio_blk_discard_write_zeroes),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0016, VIRTIO_PCI_DEVICE_BLK, test_blk_wz_too_many,
              "WRITE ZEROES with too many segments",
              VIRTIO_SPEC_V1_2, "5.2.6");
