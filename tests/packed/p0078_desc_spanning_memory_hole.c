/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0078: desc_spanning_memory_hole
 *
 * Packed twin of T0090. A data descriptor starts in a valid page but
 * claims a 256 MiB length that runs past the end of guest RAM into a
 * memory hole. The device must range check the buffer and not fault
 * when it reaches the unbacked region.
 */
#include "tests/packed/packed_util.h"

static test_result_t test_packed_desc_spanning_memory_hole(struct virtio_dev *dev,
                                                          struct vring_packed *vr)
{
    uint8_t *data = vv_alloc_pages(1);
    return pk_blk_chain(dev, vr, VIRTIO_BLK_T_IN, vv_virt_to_phys(data),
                        256 * 1024 * 1024, VRING_PACKED_DESC_F_WRITE);
}

REGISTER_TEST_PACKED(P0078, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_desc_spanning_memory_hole,
                     "Packed descriptor buffer spanning past RAM into a hole",
                     VIRTIO_SPEC_V1_2, "2.8.6");
