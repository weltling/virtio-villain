/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0066: desc_len_span_end_ram
 *
 * Packed twin of T0073. Place a packed data buffer so it ends exactly
 * at the last valid byte of guest RAM. The transfer is legal, but it
 * exercises whether the device range check is off by one at the top of
 * the memory slot. The device must complete or reject cleanly without
 * reading one byte past the slot.
 */
#include "lib/virtio_test.h"
#include "tests/packed/packed_util.h"

/* Assume 512 MiB RAM, matching the split ring twin. */
#define RAM_END 0x20000000ULL

static test_result_t test_packed_desc_len_span_end_ram(struct virtio_dev *dev,
                                                      struct vring_packed *vr)
{
    return pk_blk_chain(dev, vr, VIRTIO_BLK_T_IN, RAM_END - 512, 512,
                        VRING_PACKED_DESC_F_WRITE);
}

REGISTER_TEST_PACKED(P0066, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_desc_len_span_end_ram,
                     "Packed descriptor buffer spanning to exact end of RAM",
                     VIRTIO_SPEC_V1_2, "2.8.6");
