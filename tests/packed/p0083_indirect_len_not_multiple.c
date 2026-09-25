/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0083: indirect_len_not_multiple
 *
 * A packed indirect descriptor whose length is not a multiple of the
 * descriptor size. The indirect table is an array of fixed size
 * descriptors, so the length must divide evenly. The device must
 * reject the ragged length rather than reading a partial entry.
 */
#include "tests/packed/packed_util.h"

static test_result_t test_packed_indirect_len_not_multiple(struct virtio_dev *dev,
                                                          struct vring_packed *vr)
{
    uint64_t table_phys = pk_indirect_blk_table();

    uint16_t a0 = vr->next_avail;
    uint8_t w0 = vr->wrap_counter;

    /* Three valid entries would be 3 * 16 bytes; claim four bytes less. */
    pk_append(vr, table_phys, 3 * sizeof(struct vring_packed_desc) - 4, a0,
              VRING_PACKED_DESC_F_INDIRECT);

    return vv_kick_and_wait_packed(dev, vr, 0, a0, w0, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0083, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_indirect_len_not_multiple,
                     "Packed indirect length not a multiple of descriptor size",
                     VIRTIO_SPEC_V1_2, "2.8.6");
