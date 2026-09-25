/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0084: indirect_table_other_queue_desc
 *
 * A packed indirect descriptor whose table address points into the
 * descriptor ring of a second packed queue. That ring is device owned
 * memory, so treating it as an indirect table lets the guest steer the
 * device through another queue's state. The device must not read an
 * indirect table that overlaps another queue's ring.
 */
#include "tests/packed/packed_util.h"

static test_result_t test_packed_indirect_other_queue_desc(struct virtio_dev *dev,
                                                          struct vring_packed *vr)
{
    if (dev->common->num_queues < 2)
        return TEST_SKIP;

    struct vring_packed vr2;
    vring_packed_alloc(&vr2, 16);
    vring_packed_attach(dev, &vr2, 1);

    uint16_t a0 = vr->next_avail;
    uint8_t w0 = vr->wrap_counter;

    pk_append(vr, vr2.desc_phys,
              3 * sizeof(struct vring_packed_desc), a0,
              VRING_PACKED_DESC_F_INDIRECT);

    return vv_kick_and_wait_packed(dev, vr, 0, a0, w0, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0084, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_indirect_other_queue_desc,
                     "Packed indirect table in another queue's descriptor ring",
                     VIRTIO_SPEC_V1_2, "2.8.6");
