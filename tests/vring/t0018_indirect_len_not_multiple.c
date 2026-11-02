/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0018: indirect_len_not_multiple
 *
 * Set an indirect descriptor's len to a value that is not a multiple
 * of sizeof(struct vring_desc) (16 bytes). The spec says the length
 * must refer to a valid descriptor table.
 */
#include "lib/virtio_test.h"

static test_result_t test_indirect_len_not_multiple(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);
    struct vring_desc *indirect = vv_alloc_pages(1);

    *status = 0xFF;

    /* Fill in a valid indirect table */
    indirect[0].addr = vv_virt_to_phys(hdr);
    indirect[0].len = sizeof(*hdr);
    indirect[0].flags = VRING_DESC_F_NEXT;
    indirect[0].next = 1;
    indirect[1].addr = vv_virt_to_phys(data);
    indirect[1].len = 512;
    indirect[1].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
    indirect[1].next = 2;
    indirect[2].addr = vv_virt_to_phys(status);
    indirect[2].len = 1;
    indirect[2].flags = VRING_DESC_F_WRITE;
    indirect[2].next = 0;

    /*
     * Main descriptor: len = 47 (not a multiple of 16).
     * A valid 3-entry table would be 48 bytes.
     */
    vv_desc(vr, 0, indirect, 47, VRING_DESC_F_INDIRECT, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0018, VIRTIO_PCI_DEVICE_BLK, test_indirect_len_not_multiple,
              "Indirect descriptor len not multiple of 16",
              VIRTIO_SPEC_V1_2, "2.7.5.3");
