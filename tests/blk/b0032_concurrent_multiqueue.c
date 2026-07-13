/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0032: blk_concurrent_multiqueue
 *
 * Submit read requests simultaneously on two different queues and wait
 * for both to complete. Tests that the VMM handles concurrent I/O
 * across multiple queues without corruption or deadlock.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_concurrent_mq(struct virtio_dev *dev,
                                            struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    if (cfg->num_queues < 2)
        return TEST_SKIP;

    /*
     * Reset and reinitialize with two queues under our control.
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

    /* Prepare request on queue 0: read sector 0 */
    struct virtio_blk_outhdr *hdr0 = vv_alloc_pages(1);
    uint8_t *data0 = vv_alloc_pages(1);
    uint8_t *status0 = vv_alloc_pages(1);

    hdr0->type = VIRTIO_BLK_T_IN;
    hdr0->ioprio = 0;
    hdr0->sector = 0;
    *status0 = 0xFF;

    vring_raw_set_desc(&q0, 0, vv_virt_to_phys(hdr0), sizeof(*hdr0),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&q0, 1, vv_virt_to_phys(data0), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&q0, 2, vv_virt_to_phys(status0), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&q0, 0, 0);
    vring_raw_set_avail_idx(&q0, 1);

    /* Prepare request on queue 1: read sector 1 */
    struct virtio_blk_outhdr *hdr1 = vv_alloc_pages(1);
    uint8_t *data1 = vv_alloc_pages(1);
    uint8_t *status1 = vv_alloc_pages(1);

    hdr1->type = VIRTIO_BLK_T_IN;
    hdr1->ioprio = 0;
    hdr1->sector = 1;
    *status1 = 0xFF;

    vring_raw_set_desc(&q1, 0, vv_virt_to_phys(hdr1), sizeof(*hdr1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&q1, 1, vv_virt_to_phys(data1), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&q1, 2, vv_virt_to_phys(status1), 1,
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

REGISTER_TEST_REQUIRES(B0032, VIRTIO_PCI_DEVICE_BLK, test_blk_concurrent_mq,
              "Concurrent requests on two queues",
              VIRTIO_SPEC_V1_2, "5.2.6",
              0, 2);
