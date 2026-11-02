/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0022: blk_cross_queue_same_desc
 *
 * Two queues referencing the same guest physical memory for descriptors.
 * Tests that the VMM handles shared memory between queues without
 * corruption or crashes.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_cross_queue(struct virtio_dev *dev)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    if (cfg->num_queues < 2)
        return TEST_SKIP;

    /*
     * Reset and reinitialize so we can configure queues before DRIVER_OK.
     */
    virtio_pci_reset(dev);

    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /* Accept offered word 0 and the base transport features. */
    cfg->device_feature_select = 0;
    __sync_synchronize();
    cfg->driver_feature_select = 0;
    cfg->driver_feature = cfg->device_feature;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = virtio_pci_base_features_hi(dev);
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        return TEST_SKIP;

    /* Set up queue 0 normally */
    struct vring q0;
    vring_alloc(&q0, 64);
    vring_attach(dev, &q0, 0);

    /*
     * Set up queue 1 with the same descriptor table physical address
     * as queue 0. This is invalid - queues should have independent
     * memory regions.
     */
    cfg->queue_select = 1;
    __sync_synchronize();
    cfg->queue_size = q0.size;
    virtio_store64(&cfg->queue_desc, q0.desc_phys);
    virtio_store64(&cfg->queue_avail, q0.avail_phys);
    virtio_store64(&cfg->queue_used, q0.used_phys);
    cfg->queue_msix_vector = 0xffff;
    __sync_synchronize();
    cfg->queue_enable = 1;
    __sync_synchronize();

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Now submit a request on queue 0 */
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(&q0, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(&q0, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(&q0, 2, status, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&q0, 0, 0);
    vring_raw_set_avail_idx(&q0, 1);

    /* Kick both queues with same descriptor state */
    virtio_pci_kick(dev, 1);
    return vv_kick_and_wait(dev, &q0, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_DEV_REQUIRES(B0022, VIRTIO_PCI_DEVICE_BLK, test_blk_cross_queue,
                  "Two queues sharing same descriptor memory",
                  VIRTIO_SPEC_V1_2, "5.2.6",
                  0, 2);
