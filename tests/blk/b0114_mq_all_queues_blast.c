/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0114: blk_mq_all_queues_blast
 *
 * Submit one request on every available queue simultaneously and
 * kick all. Tests multiqueue stress - exercises the VMM's ability
 * to handle concurrent I/O on all queues at once.
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
#define MAX_QUEUES 16

static test_result_t test_blk_mq_all_blast(struct virtio_dev *dev,
                                           struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;

    if (nq < 2)
        return TEST_SKIP;
    if (nq > MAX_QUEUES)
        nq = MAX_QUEUES;

    /*
     * Reset and reinitialize with all queues.
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

    struct vring queues[MAX_QUEUES];
    for (uint16_t i = 0; i < nq; i++) {
        vring_alloc(&queues[i], 16);
        vring_attach(dev, &queues[i], i);
    }

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    (void)vr;

    /* Submit a read request on each queue */
    for (uint16_t i = 0; i < nq; i++) {
        struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
        uint8_t *data = vv_alloc_pages(1);
        uint8_t *status = vv_alloc_pages(1);

        hdr->type = VIRTIO_BLK_T_IN;
        hdr->ioprio = 0;
        hdr->sector = i; /* different sector per queue */
        *status = 0xFF;

        vring_raw_set_desc(&queues[i], 0, vv_virt_to_phys(hdr),
                           sizeof(*hdr), VRING_DESC_F_NEXT, 1);
        vring_raw_set_desc(&queues[i], 1, vv_virt_to_phys(data), 512,
                           VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
        vring_raw_set_desc(&queues[i], 2, vv_virt_to_phys(status), 1,
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(&queues[i], 0, 0);
        vring_raw_set_avail_idx(&queues[i], 1);
    }

    /* Kick all queues back-to-back */
    __sync_synchronize();
    for (uint16_t i = 0; i < nq; i++)
        virtio_pci_kick(dev, i);

    /* Wait for all to complete */
    int elapsed = 0;
    int step = 10000;
    while (elapsed < 2000000) {
        usleep(step);
        __sync_synchronize();
        int all_done = 1;
        for (uint16_t i = 0; i < nq; i++) {
            if (queues[i].used->idx == 0) {
                all_done = 0;
                break;
            }
        }
        if (all_done)
            return TEST_PASS;
        elapsed += step;
    }

    __sync_synchronize();
    uint8_t dev_status = cfg->device_status;
    if (dev_status == 0)
        TWEDGED("dev_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(B0114, VIRTIO_PCI_DEVICE_BLK, test_blk_mq_all_blast,
              "Submit requests on all queues simultaneously",
              VIRTIO_SPEC_V1_2, "5.2.6");
