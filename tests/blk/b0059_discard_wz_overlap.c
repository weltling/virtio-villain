/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0059: Concurrent DISCARD and WRITE_ZEROES to overlapping sectors (spec 5.2.6.2)
 *
 * Submit both a DISCARD and a WRITE_ZEROES targeting the same sector
 * range, in-flight simultaneously. Tests device handling of conflicting
 * erase-like operations.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_discard_wz_overlap(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;

    if (!(offered & (1U << VIRTIO_BLK_F_DISCARD)))
        return TEST_SKIP;
    if (!(offered & (1U << VIRTIO_BLK_F_WRITE_ZEROES)))
        return TEST_SKIP;
    if (cfg->num_queues < 2)
        return TEST_SKIP;

    /* Set up second queue */
    struct vring vr2;
    vring_alloc(&vr2, 16);
    vring_attach(dev, &vr2, 1);

    /* DISCARD on queue 0 */
    struct virtio_blk_outhdr *hdr0 = vv_alloc_pages(1);
    struct virtio_blk_discard_write_zeroes *seg0 = vv_alloc_pages(1);
    uint8_t *st0 = vv_alloc_pages(1);

    hdr0->type = VIRTIO_BLK_T_DISCARD;
    hdr0->ioprio = 0;
    hdr0->sector = 0;
    seg0->sector = 0;
    seg0->num_sectors = 8;
    seg0->flags = 0;
    *st0 = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr0), sizeof(*hdr0),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(seg0), sizeof(*seg0),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st0), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    /* WRITE_ZEROES on queue 1 - same sector range */
    struct virtio_blk_outhdr *hdr1 = vv_alloc_pages(1);
    struct virtio_blk_discard_write_zeroes *seg1 = vv_alloc_pages(1);
    uint8_t *st1 = vv_alloc_pages(1);

    hdr1->type = VIRTIO_BLK_T_WRITE_ZEROES;
    hdr1->ioprio = 0;
    hdr1->sector = 0;
    seg1->sector = 0;
    seg1->num_sectors = 8;
    seg1->flags = 0;
    *st1 = 0xFF;

    vring_raw_set_desc(&vr2, 0, vv_virt_to_phys(hdr1), sizeof(*hdr1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&vr2, 1, vv_virt_to_phys(seg1), sizeof(*seg1),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&vr2, 2, vv_virt_to_phys(st1), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&vr2, 0, 0);
    vring_raw_set_avail_idx(&vr2, 1);

    /* Kick both simultaneously */
    __sync_synchronize();
    uint16_t before0 = vr->used->idx;
    uint16_t before1 = vr2.used->idx;
    virtio_pci_kick(dev, 0);
    virtio_pci_kick(dev, 1);

    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if (vr->used->idx != before0 && vr2.used->idx != before1)
            return TEST_PASS;
        elapsed += 10000;
    }

    __sync_synchronize();
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST_REQUIRES(B0059, VIRTIO_PCI_DEVICE_BLK, test_blk_discard_wz_overlap,
              "Concurrent DISCARD and WRITE_ZEROES to same sectors",
              VIRTIO_SPEC_V1_2, "5.2.6.2",
              (1ULL << VIRTIO_BLK_F_DISCARD) | (1ULL << VIRTIO_BLK_F_WRITE_ZEROES), 2);
