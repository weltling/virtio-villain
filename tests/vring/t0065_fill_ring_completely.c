/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0065: fill_ring_completely
 *
 * Post queue_size buffers (using 1 descriptor each with indirect) to
 * completely fill the available ring. Tests that the device can
 * process a full ring without off-by-one in the available/used
 * index wrap handling.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_fill_ring_completely(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint16_t qsz = vr->size;

    /*
     * Use indirect descriptors so each avail ring entry only
     * consumes 1 descriptor slot. This lets us fill all qsz slots.
     */
    struct vring_desc *tables = vv_alloc_pages(16);
    struct virtio_blk_outhdr *hdrs = vv_alloc_pages(4);
    uint8_t *datas = vv_alloc_pages(qsz);
    uint8_t *statuses = vv_alloc_pages(1);

    for (uint16_t i = 0; i < qsz; i++) {
        struct virtio_blk_outhdr *h = &hdrs[i];
        h->type = VIRTIO_BLK_T_IN;
        h->ioprio = 0;
        h->sector = i % 100;

        uint8_t *d = datas + (i * 512);
        uint8_t *s = statuses + i;
        *s = 0xFF;

        /* Each request gets its own 3-entry indirect table */
        struct vring_desc *tbl = &tables[i * 3];
        tbl[0].addr = vv_virt_to_phys(h);
        tbl[0].len = sizeof(*h);
        tbl[0].flags = VRING_DESC_F_NEXT;
        tbl[0].next = 1;

        tbl[1].addr = vv_virt_to_phys(d);
        tbl[1].len = 512;
        tbl[1].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
        tbl[1].next = 2;

        tbl[2].addr = vv_virt_to_phys(s);
        tbl[2].len = 1;
        tbl[2].flags = VRING_DESC_F_WRITE;
        tbl[2].next = 0;

        /* Ring descriptor points at indirect table */
        vring_raw_set_desc(vr, i, vv_virt_to_phys(tbl),
                           3 * sizeof(struct vring_desc),
                           VRING_DESC_F_INDIRECT, 0);

        vring_raw_set_avail(vr, i, i);
    }

    vring_raw_set_avail_idx(vr, qsz);

    return vv_kick_and_wait(dev, vr, 0, 2000);
}

REGISTER_TEST(T0065, VIRTIO_PCI_DEVICE_BLK, test_fill_ring_completely,
              "Fill ring completely (queue_size indirect buffers)",
              VIRTIO_SPEC_V1_2, "2.7.7");
