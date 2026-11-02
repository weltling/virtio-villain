/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0021: indirect_chain_loop
 *
 * Create a loop within the indirect descriptor table. Descriptor 1's
 * next points back to descriptor 0, forming a cycle.
 */
#include "lib/virtio_test.h"

static test_result_t test_indirect_chain_loop(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    struct vring_desc *indirect = vv_alloc_pages(1);

    uint64_t indirect_phys = vv_virt_to_phys(indirect);

    /* Indirect table with a loop: [0] -> [1] -> [0] */
    indirect[0].addr = vv_virt_to_phys(hdr);
    indirect[0].len = sizeof(*hdr);
    indirect[0].flags = VRING_DESC_F_NEXT;
    indirect[0].next = 1;
    indirect[1].addr = vv_virt_to_phys(data);
    indirect[1].len = 512;
    indirect[1].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
    indirect[1].next = 0; /* loop back to [0] */

    /* Main descriptor */
    vring_raw_set_desc(vr, 0, indirect_phys, 2 * sizeof(struct vring_desc),
                       VRING_DESC_F_INDIRECT, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0021, VIRTIO_PCI_DEVICE_BLK, test_indirect_chain_loop,
              "Loop in indirect descriptor table",
              VIRTIO_SPEC_V1_2, "2.7.5.3");
