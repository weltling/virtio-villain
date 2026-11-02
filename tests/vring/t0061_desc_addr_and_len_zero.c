/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0061: desc_addr_and_len_zero
 *
 * Create a descriptor with both addr=0 and len=0. This is a degenerate
 * case - the descriptor contributes nothing to the chain but a VMM
 * that dereferences addr without checking len may fault.
 */
#include "lib/virtio_test.h"

static test_result_t test_desc_addr_and_len_zero(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* desc 0: valid header */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    /* desc 1: addr=0, len=0, writable - degenerate data descriptor */
    vring_raw_set_desc(vr, 1, 0, 0,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    /* desc 2: status */
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0061, VIRTIO_PCI_DEVICE_BLK, test_desc_addr_and_len_zero,
              "Descriptor with addr=0 and len=0 simultaneously",
              VIRTIO_SPEC_V1_2, "2.7.5");
