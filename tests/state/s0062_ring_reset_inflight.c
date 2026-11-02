/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0062: ring_reset_with_inflight_descriptors
 *
 * Submit descriptors to a queue, then immediately reset that queue
 * via RING_RESET before the device completes the request. Spec 2.2.1:
 * after reset, the device MUST NOT execute requests from that queue
 * or notify the driver for it.
 */
#include "lib/virtio_test.h"

static test_result_t test_ring_reset_inflight(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Check if RING_RESET is offered */
    cfg->device_feature_select = VIRTIO_F_RING_RESET / 32;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << (VIRTIO_F_RING_RESET % 32))))
        return TEST_SKIP;

    /* Submit a request but do NOT wait for completion */
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    vring_submit(vr, 0);
    virtio_pci_kick(dev, 0);

    /* Immediately reset the queue without waiting */
    if (virtio_pci_queue_reset(dev, 0) < 0)
        TFAIL("queue_enable not cleared after reset");

    /* Queue must now be disabled */
    if (cfg->queue_enable != 0)
        TFAIL("cfg->queue_enable != 0");

    /* Device must not have crashed */
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(S0062, VIRTIO_PCI_DEVICE_BLK, test_ring_reset_inflight,
              "RING_RESET with inflight descriptors",
              VIRTIO_SPEC_V1_3, "2.2.1",
              (1ULL << VIRTIO_F_RING_RESET), 0);
