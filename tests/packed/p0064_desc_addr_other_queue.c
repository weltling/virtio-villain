/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0064: desc_addr_other_queue
 *
 * Packed twin of T0107. Point a device writable data descriptor on
 * queue 0 at the descriptor ring of a second packed queue. That ring
 * is device owned memory, so a blk read that DMAs disk bytes into it
 * lets the guest forge ring state on a queue it is not driving. The
 * device must not honor an address that lands on another queue's ring.
 */
#include "lib/virtio_test.h"
#include "lib/packed_util.h"

static test_result_t test_packed_desc_addr_other_queue(struct virtio_dev *dev,
                                                      struct vring_packed *vr)
{
    if (dev->common->num_queues < 2)
        return TEST_SKIP;

    struct vring_packed vr2;
    vring_packed_alloc(&vr2, 16);
    vring_packed_attach(dev, &vr2, 1);

    return pk_blk_chain(dev, vr, VIRTIO_BLK_T_IN, vr2.desc_phys, 512,
                        VRING_PACKED_DESC_F_WRITE);
}

REGISTER_TEST_PACKED(P0064, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_desc_addr_other_queue,
                     "Packed descriptor address into another queue's ring",
                     VIRTIO_SPEC_V1_2, "2.8.6");
