/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0072: desc_addr_len_zero
 *
 * Present a packed data descriptor with both addr and len set to zero.
 * P0047 covers addr zero with a nonzero length; here the descriptor is
 * a zero length buffer at the null address, a degenerate entry. The
 * device must neither dereference the null address nor mishandle the
 * empty segment, and must stay healthy.
 */
#include "lib/virtio_test.h"
#include "lib/packed_util.h"

static test_result_t test_packed_desc_addr_len_zero(struct virtio_dev *dev,
                                                   struct vring_packed *vr)
{
    return pk_blk_chain(dev, vr, VIRTIO_BLK_T_IN, 0, 0,
                        VRING_PACKED_DESC_F_WRITE);
}

REGISTER_TEST_PACKED(P0072, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_desc_addr_len_zero,
                     "Packed descriptor with addr and len both zero",
                     VIRTIO_SPEC_V1_2, "2.8.6");
