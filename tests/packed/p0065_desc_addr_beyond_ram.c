/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0065: desc_addr_beyond_ram
 *
 * Packed twin of T0005. Point a packed data descriptor at a guest
 * physical address far beyond the VM's RAM. A device that translates
 * the GPA to a host pointer without a bounds check dereferences an
 * invalid pointer and faults the VMM. The device must reject the
 * address instead of accessing it.
 */
#include "lib/virtio_test.h"
#include "lib/packed_util.h"

static test_result_t test_packed_desc_addr_beyond_ram(struct virtio_dev *dev,
                                                     struct vring_packed *vr)
{
    return pk_blk_chain(dev, vr, VIRTIO_BLK_T_IN, 0xFFFFFFFFFFFF0000ULL, 512,
                        VRING_PACKED_DESC_F_WRITE);
}

REGISTER_TEST_PACKED(P0065, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_desc_addr_beyond_ram,
                     "Packed descriptor GPA beyond guest RAM",
                     VIRTIO_SPEC_V1_2, "2.8.6");
