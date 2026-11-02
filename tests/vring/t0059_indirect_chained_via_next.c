/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0059: indirect_chained_via_next
 *
 * Create two descriptors in the ring, both with INDIRECT flag, chained
 * via NEXT. The spec says MUST NOT set both INDIRECT and NEXT, but this
 * tests the case where descriptor 0 is INDIRECT|NEXT pointing to
 * descriptor 1 which is also INDIRECT - two indirect tables in one chain.
 */
#include "lib/virtio_test.h"

static test_result_t test_indirect_chained(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* First indirect table: just the header descriptor */
    struct vring_desc *indirect1 = vv_alloc_pages(1);
    indirect1[0].addr = vv_virt_to_phys(hdr);
    indirect1[0].len = sizeof(*hdr);
    indirect1[0].flags = 0;
    indirect1[0].next = 0;

    /* Second indirect table: data + status */
    struct vring_desc *indirect2 = vv_alloc_pages(1);
    indirect2[0].addr = vv_virt_to_phys(data);
    indirect2[0].len = 512;
    indirect2[0].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
    indirect2[0].next = 1;
    indirect2[1].addr = vv_virt_to_phys(status);
    indirect2[1].len = 1;
    indirect2[1].flags = VRING_DESC_F_WRITE;
    indirect2[1].next = 0;

    uint64_t ind1_phys = vv_virt_to_phys(indirect1);
    uint64_t ind2_phys = vv_virt_to_phys(indirect2);

    /*
     * Ring descriptor 0: INDIRECT | NEXT -> points to indirect1
     * Ring descriptor 1: INDIRECT -> points to indirect2
     * This violates "MUST NOT set both INDIRECT and NEXT in flags"
     */
    vring_raw_set_desc(vr, 0, ind1_phys, 1 * sizeof(struct vring_desc),
                       VRING_DESC_F_INDIRECT | VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, ind2_phys, 2 * sizeof(struct vring_desc),
                       VRING_DESC_F_INDIRECT, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0059, VIRTIO_PCI_DEVICE_BLK, test_indirect_chained,
              "Two indirect descriptors chained via NEXT in ring",
              VIRTIO_SPEC_V1_2, "2.7.5.3");
