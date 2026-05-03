/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0081: concurrent_kicks_two_queues
 *
 * Submit a request on two different queues (if the device supports
 * multiple queues) and kick both before waiting for completion.
 * Tests device handling of concurrent queue processing.
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

static test_result_t test_concurrent_kicks_two_queues(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;

    if (nq < 2)
        return TEST_SKIP; /* need at least 2 queues */

    /* First queue already set up as vr (queue 0). Set up queue 1. */
    struct vring vr2;
    vring_alloc(&vr2, 16);
    vring_attach(dev, &vr2, 1);

    /* Prepare request on queue 0 */
    struct virtio_blk_outhdr *hdr0 = vv_alloc_pages(1);
    uint8_t *data0 = vv_alloc_pages(1);
    uint8_t *st0 = vv_alloc_pages(1);

    hdr0->type = VIRTIO_BLK_T_IN;
    hdr0->ioprio = 0;
    hdr0->sector = 0;
    *st0 = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr0), sizeof(*hdr0),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data0), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st0), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    /* Prepare request on queue 1 */
    struct virtio_blk_outhdr *hdr1 = vv_alloc_pages(1);
    uint8_t *data1 = vv_alloc_pages(1);
    uint8_t *st1 = vv_alloc_pages(1);

    hdr1->type = VIRTIO_BLK_T_IN;
    hdr1->ioprio = 0;
    hdr1->sector = 1;
    *st1 = 0xFF;

    vring_raw_set_desc(&vr2, 0, vv_virt_to_phys(hdr1), sizeof(*hdr1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&vr2, 1, vv_virt_to_phys(data1), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&vr2, 2, vv_virt_to_phys(st1), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&vr2, 0, 0);
    vring_raw_set_avail_idx(&vr2, 1);

    /* Kick both queues back-to-back */
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

    /* At least one didn't complete */
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(T0081, VIRTIO_PCI_DEVICE_BLK, test_concurrent_kicks_two_queues,
              "Concurrent kicks on two separate queues",
              VIRTIO_SPEC_V1_2, "2.7.13");
