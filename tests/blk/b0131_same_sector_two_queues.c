/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0131: Concurrent writes to same sector from two queues.
 *
 * Submit write requests to the same sector from two different queues
 * simultaneously. Tests device-side serialization / race handling.
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

static test_result_t test_blk_same_sector_two_queues(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;

    if (nq < 2)
        return TEST_SKIP;

    (void)vr;

    /* Reset and reinit with 2 queues */
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

    struct vring vr0, vr1;
    vring_alloc(&vr0, 16);
    vring_alloc(&vr1, 16);
    vring_attach(dev, &vr0, 0);
    vring_attach(dev, &vr1, 1);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    struct virtio_blk_outhdr *hdr0 = vv_alloc_pages(1);
    uint8_t *data0 = vv_alloc_pages(1);
    uint8_t *status0 = vv_alloc_pages(1);
    struct virtio_blk_outhdr *hdr1 = vv_alloc_pages(1);
    uint8_t *data1 = vv_alloc_pages(1);
    uint8_t *status1 = vv_alloc_pages(1);

    hdr0->type = VIRTIO_BLK_T_OUT;
    hdr0->ioprio = 0;
    hdr0->sector = 0;
    memset(data0, 0xAA, 512);
    *status0 = 0xFF;

    hdr1->type = VIRTIO_BLK_T_OUT;
    hdr1->ioprio = 0;
    hdr1->sector = 0;
    memset(data1, 0xBB, 512);
    *status1 = 0xFF;

    /* Queue 0 */
    vring_raw_set_desc(&vr0, 0, vv_virt_to_phys(hdr0), sizeof(*hdr0),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&vr0, 1, vv_virt_to_phys(data0), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&vr0, 2, vv_virt_to_phys(status0), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&vr0, 0, 0);
    vring_raw_set_avail_idx(&vr0, 1);

    /* Queue 1 */
    vring_raw_set_desc(&vr1, 0, vv_virt_to_phys(hdr1), sizeof(*hdr1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&vr1, 1, vv_virt_to_phys(data1), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&vr1, 2, vv_virt_to_phys(status1), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&vr1, 0, 0);
    vring_raw_set_avail_idx(&vr1, 1);

    __sync_synchronize();
    virtio_pci_kick(dev, vr0.queue);
    virtio_pci_kick(dev, vr1.queue);

    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if (vr0.used->idx >= 1 && vr1.used->idx >= 1)
            return TEST_PASS;
        elapsed += 10000;
    }

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(B0131, VIRTIO_PCI_DEVICE_BLK, test_blk_same_sector_two_queues,
              "Concurrent writes to same sector from two queues",
              VIRTIO_SPEC_V1_2, "5.2.6");
