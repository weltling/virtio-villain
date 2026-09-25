/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0069: indirect_len_zero
 *
 * A packed indirect descriptor whose len field is zero, implying an
 * indirect table with no entries. The spec requires the length to be a
 * nonzero multiple of the descriptor size. The device must reject it
 * rather than reading an empty or unbounded table.
 */
#include "tests/packed/packed_util.h"

static test_result_t test_packed_indirect_len_zero(struct virtio_dev *dev,
                                                  struct vring_packed *vr)
{
    struct vring_packed_desc *tab = vv_alloc_pages(1);

    uint16_t a0 = vr->next_avail;
    uint8_t w0 = vr->wrap_counter;

    pk_append(vr, vv_virt_to_phys(tab), 0, a0,
              VRING_PACKED_DESC_F_INDIRECT);

    return vv_kick_and_wait_packed(dev, vr, 0, a0, w0, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0069, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_indirect_len_zero,
                     "Packed indirect descriptor with zero length",
                     VIRTIO_SPEC_V1_2, "2.8.6");
