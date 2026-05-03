/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0054: concurrent_conflicting_writes_same_sector
 *
 * Submit two write commands to the same sector from two different
 * queues simultaneously. Tests device handling of write conflicts.
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

#define VIRTIO_BLK_T_OUT 1

static test_result_t test_blk_concurrent_write_conflict(struct virtio_dev *dev,
                                                        struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;

    if (nq < 2)
        return TEST_SKIP;

    /* Set up second queue */
    struct vring vr2;
    vring_alloc(&vr2, 16);
    vring_attach(dev, &vr2, 1);

    /* Write request on queue 0: fill sector 0 with 0xAA */
    struct virtio_blk_outhdr *hdr0 = vv_alloc_pages(1);
    uint8_t *data0 = vv_alloc_pages(1);
    uint8_t *st0 = vv_alloc_pages(1);

    hdr0->type = VIRTIO_BLK_T_OUT;
    hdr0->ioprio = 0;
    hdr0->sector = 0;
    memset(data0, 0xAA, 512);
    *st0 = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr0), sizeof(*hdr0),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data0), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st0), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    /* Write request on queue 1: fill sector 0 with 0xBB */
    struct virtio_blk_outhdr *hdr1 = vv_alloc_pages(1);
    uint8_t *data1 = vv_alloc_pages(1);
    uint8_t *st1 = vv_alloc_pages(1);

    hdr1->type = VIRTIO_BLK_T_OUT;
    hdr1->ioprio = 0;
    hdr1->sector = 0;
    memset(data1, 0xBB, 512);
    *st1 = 0xFF;

    vring_raw_set_desc(&vr2, 0, vv_virt_to_phys(hdr1), sizeof(*hdr1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&vr2, 1, vv_virt_to_phys(data1), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&vr2, 2, vv_virt_to_phys(st1), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&vr2, 0, 0);
    vring_raw_set_avail_idx(&vr2, 1);

    /* Kick both queues simultaneously */
    __sync_synchronize();
    uint16_t before0 = vr->used->idx;
    uint16_t before1 = vr2.used->idx;
    virtio_pci_kick(dev, 0);
    virtio_pci_kick(dev, 1);

    /* Wait for both to complete */
    int elapsed = 0;
    int done0 = 0, done1 = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if (!done0 && vr->used->idx != before0)
            done0 = 1;
        if (!done1 && vr2.used->idx != before1)
            done1 = 1;
        if (done0 && done1)
            return TEST_PASS;
        elapsed += 10000;
    }

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(B0054, VIRTIO_PCI_DEVICE_BLK, test_blk_concurrent_write_conflict,
              "Concurrent writes to same sector from two queues",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
