/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0163: blk_discard_then_read_same
 *
 * Submit a DISCARD over sectors 0..7, then immediately submit a
 * READ of sector 0 in the same avail ring batch. Spec 5.2.6.2
 * says DISCARD MAY deallocate the range; the device must not
 * crash or return stale UAF backing when a read races immediately
 * behind the unmap. Result data is not asserted because spec does
 * not mandate zero return after DISCARD.
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

#define VIRTIO_BLK_T_IN      0
#define VIRTIO_BLK_T_DISCARD 11

#define VIRTIO_BLK_F_DISCARD 13

static test_result_t test_blk_discard_then_read_same(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_BLK_F_DISCARD)))
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr1 = vv_alloc_pages(1);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status1 = vv_alloc_pages(1);

    struct virtio_blk_outhdr *hdr2 = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status2 = vv_alloc_pages(1);

    hdr1->type = VIRTIO_BLK_T_DISCARD;
    hdr1->ioprio = 0;
    hdr1->sector = 0;
    seg->sector = 0;
    seg->num_sectors = 8;
    seg->flags = 0;
    *status1 = 0xFF;

    hdr2->type = VIRTIO_BLK_T_IN;
    hdr2->ioprio = 0;
    hdr2->sector = 0;
    memset(data, 0xAA, 4096);
    *status2 = 0xFF;

    /* Chain 1: DISCARD (descs 0,1,2) */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr1), sizeof(*hdr1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(seg), sizeof(*seg),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status1), 1,
                       VRING_DESC_F_WRITE, 0);

    /* Chain 2: READ sector 0, 4KB (descs 3,4,5) */
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(hdr2), sizeof(*hdr2),
                       VRING_DESC_F_NEXT, 4);
    vring_raw_set_desc(vr, 4, vv_virt_to_phys(data), 4096,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 5);
    vring_raw_set_desc(vr, 5, vv_virt_to_phys(status2), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 3);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0163, VIRTIO_PCI_DEVICE_BLK, test_blk_discard_then_read_same,
              "Discard sector range then read same sector in one batch",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
