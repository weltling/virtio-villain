/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0112: blk_concurrent_discard_write
 *
 * Submit discard and write to overlapping sector range on separate
 * queues simultaneously. Tests race between discard and write to the
 * same region.
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

#define VIRTIO_BLK_T_OUT     1
#define VIRTIO_BLK_T_DISCARD 11

static test_result_t test_blk_concurrent_discard_write(struct virtio_dev *dev,
                                                       struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    if (cfg->num_queues < 2)
        return TEST_SKIP;

    /*
     * Reset and reinitialize with two queues.
     */
    virtio_pci_reset(dev);

    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->device_feature_select = 0;
    __sync_synchronize();
    cfg->driver_feature_select = 0;
    cfg->driver_feature = cfg->device_feature;
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        return TEST_SKIP;

    struct vring q0, q1;
    vring_alloc(&q0, 64);
    vring_alloc(&q1, 64);
    vring_attach(dev, &q0, 0);
    vring_attach(dev, &q1, 1);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    (void)vr;

    /* Queue 0: discard sectors 0-7 */
    struct virtio_blk_outhdr *dhdr = vv_alloc_pages(1);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *dstatus = vv_alloc_pages(1);

    dhdr->type = VIRTIO_BLK_T_DISCARD;
    dhdr->ioprio = 0;
    dhdr->sector = 0;

    seg->sector = 0;
    seg->num_sectors = 8;
    seg->flags = 0;
    *dstatus = 0xFF;

    vring_raw_set_desc(&q0, 0, vv_virt_to_phys(dhdr), sizeof(*dhdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&q0, 1, vv_virt_to_phys(seg), sizeof(*seg),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&q0, 2, vv_virt_to_phys(dstatus), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&q0, 0, 0);
    vring_raw_set_avail_idx(&q0, 1);

    /* Queue 1: write to sector 0 (overlapping) */
    struct virtio_blk_outhdr *whdr = vv_alloc_pages(1);
    uint8_t *wdata = vv_alloc_pages(1);
    uint8_t *wstatus = vv_alloc_pages(1);

    whdr->type = VIRTIO_BLK_T_OUT;
    whdr->ioprio = 0;
    whdr->sector = 0;
    memset(wdata, 0xBB, 512);
    *wstatus = 0xFF;

    vring_raw_set_desc(&q1, 0, vv_virt_to_phys(whdr), sizeof(*whdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&q1, 1, vv_virt_to_phys(wdata), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&q1, 2, vv_virt_to_phys(wstatus), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&q1, 0, 0);
    vring_raw_set_avail_idx(&q1, 1);

    /* Kick both queues simultaneously */
    __sync_synchronize();
    virtio_pci_kick(dev, 0);
    virtio_pci_kick(dev, 1);

    /* Wait for both to complete */
    int elapsed = 0;
    int step = 10000;
    int done0 = 0, done1 = 0;
    while (elapsed < 1000000) {
        usleep(step);
        __sync_synchronize();
        if (!done0 && q0.used->idx != 0)
            done0 = 1;
        if (!done1 && q1.used->idx != 0)
            done1 = 1;
        if (done0 && done1)
            return TEST_PASS;
        elapsed += step;
    }

    __sync_synchronize();
    uint8_t dev_status = cfg->device_status;
    if (dev_status == 0)
        TWEDGED("dev_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(B0112, VIRTIO_PCI_DEVICE_BLK, test_blk_concurrent_discard_write,
              "Concurrent discard and write to overlapping sectors",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
