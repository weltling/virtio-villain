/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0022: blk_cross_queue_same_desc
 *
 * Two queues referencing the same guest physical memory for descriptors.
 * Tests that the VMM handles shared memory between queues without
 * corruption or crashes.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_cross_queue(struct virtio_dev *dev,
                                          struct vring *vr)
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

    /* Accept whatever features the device offers (low word only) */
    cfg->device_feature_select = 0;
    __sync_synchronize();
    cfg->driver_feature_select = 0;
    cfg->driver_feature = cfg->device_feature;
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

    (void)vr;

    /* Now submit a request on queue 0 */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(&q0, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&q0, 1, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&q0, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&q0, 0, 0);
    vring_raw_set_avail_idx(&q0, 1);

    /* Kick both queues with same descriptor state */
    virtio_pci_kick(dev, 1);
    return vv_kick_and_wait(dev, &q0, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(B0022, VIRTIO_PCI_DEVICE_BLK, test_blk_cross_queue,
              "Two queues sharing same descriptor memory",
              VIRTIO_SPEC_V1_2, "5.2.6",
              0, 2);
