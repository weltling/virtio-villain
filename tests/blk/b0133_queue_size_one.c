/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0133: Queue size=1 with full request chain.
 *
 * Attempt to submit a full block request (header + data + status = 3
 * descriptors) on a queue that was configured with size 1. The device
 * cannot complete this chain.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_queue_size_one(struct virtio_dev *dev,
                                             struct vring *vr)
{
    /*
     * Configure a queue of size 1 by writing queue_size=1 via MMIO/PCI.
     * This is only valid if the device supports queue_size negotiation.
     * With queue_size=1, only descriptor index 0 exists.
     */
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /*
     * Post 3 descriptors even though ring might only have 1 slot.
     * The device must detect the overflow or invalid next index.
     */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0133, VIRTIO_PCI_DEVICE_BLK, test_blk_queue_size_one,
              "Full request chain on queue with size=1",
              VIRTIO_SPEC_V1_2, "5.2.6");
