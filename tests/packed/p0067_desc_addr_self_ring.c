/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0067: desc_addr_self_ring
 *
 * Packed twin of T0070. Point a device writable data descriptor at the
 * queue's own descriptor ring. A device that DMAs the read data there
 * overwrites live descriptor entries and may corrupt its own parsing or
 * loop. The device must not write into the descriptor ring it is
 * reading from.
 */
#include "lib/virtio_test.h"
#include "tests/packed/packed_util.h"

static test_result_t test_packed_desc_addr_self_ring(struct virtio_dev *dev,
                                                    struct vring_packed *vr)
{
    return pk_blk_chain(dev, vr, VIRTIO_BLK_T_IN, vr->desc_phys, 512,
                        VRING_PACKED_DESC_F_WRITE);
}

REGISTER_TEST_PACKED(P0067, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_desc_addr_self_ring,
                     "Packed descriptor address into own descriptor ring",
                     VIRTIO_SPEC_V1_2, "2.8.6");
