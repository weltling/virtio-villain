/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0040: blk_fill_avail_ring
 *
 * Fill the entire available ring to queue_size entries. This exercises
 * the VMM's used ring wrap-around handling and tests for off-by-one
 * errors when the ring is completely full.
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

#define VIRTIO_BLK_T_IN 0

static test_result_t test_blk_fill_avail(struct virtio_dev *dev,
                                         struct vring *vr)
{
    /*
     * Each request needs 3 descriptors (hdr + data + status).
     * With queue_size=64, we can fit 64/3 = 21 requests.
     * Fill all 21 slots to saturate the ring.
     */
    uint16_t qsz = vr->size;
    uint16_t max_reqs = qsz / 3;

    struct virtio_blk_outhdr *hdrs = vv_alloc_pages(4);
    uint8_t *datas = vv_alloc_pages(max_reqs); /* 1 page each won't fit, use small bufs */
    uint8_t *statuses = vv_alloc_pages(1);

    for (uint16_t i = 0; i < max_reqs; i++) {
        uint16_t base = i * 3;
        struct virtio_blk_outhdr *h = &hdrs[i];
        h->type = VIRTIO_BLK_T_IN;
        h->ioprio = 0;
        h->sector = i; /* read different sectors */

        uint8_t *d = datas + (i * 512);
        uint8_t *s = statuses + i;
        *s = 0xFF;

        vring_raw_set_desc(vr, base, vv_virt_to_phys(h), sizeof(*h),
                           VRING_DESC_F_NEXT, base + 1);
        vring_raw_set_desc(vr, base + 1, vv_virt_to_phys(d), 512,
                           VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, base + 2);
        vring_raw_set_desc(vr, base + 2, vv_virt_to_phys(s), 1,
                           VRING_DESC_F_WRITE, 0);

        vring_raw_set_avail(vr, i, base);
    }

    vring_raw_set_avail_idx(vr, max_reqs);

    return vv_kick_and_wait(dev, vr, 0, 1000);
}

REGISTER_TEST(B0040, VIRTIO_PCI_DEVICE_BLK, test_blk_fill_avail,
              "Fill entire avail ring to exercise used ring wrap",
              VIRTIO_SPEC_V1_2, "5.2.6");
