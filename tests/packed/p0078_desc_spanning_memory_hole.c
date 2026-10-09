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
    uint64_t ram_top = vv_parse_ram_top();
    uint64_t data_phys;
    if (!ram_top ||
        !vv_alloc_page_near_ram_top(ram_top, &data_phys))
        return TEST_SKIP;

    uint64_t span = ram_top - data_phys + PAGE_SIZE;
    if (span > UINT32_MAX)
        return TEST_SKIP;

    return pk_blk_chain(dev, vr, VIRTIO_BLK_T_IN, data_phys, (uint32_t)span,
                        VRING_PACKED_DESC_F_WRITE);
}

REGISTER_TEST_PACKED(P0078, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_desc_spanning_memory_hole,
                     "Packed descriptor buffer spanning past RAM into a hole",
                     VIRTIO_SPEC_V1_2, "2.8.6");
