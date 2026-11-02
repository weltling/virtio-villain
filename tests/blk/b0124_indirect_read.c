/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0124: Read request via indirect descriptor table.
 *
 * Submit a read using an indirect descriptor table containing the
 * header, data, and status descriptors.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_indirect_read(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* Build indirect descriptor table (3 entries, 16 bytes each) */
    struct vring_desc *indirect = vv_alloc_pages(1);

    indirect[0].addr = vv_virt_to_phys(hdr);
    indirect[0].len = sizeof(*hdr);
    indirect[0].flags = VRING_DESC_F_NEXT;
    indirect[0].next = 1;

    indirect[1].addr = vv_virt_to_phys(data);
    indirect[1].len = 512;
    indirect[1].flags = VRING_DESC_F_WRITE | VRING_DESC_F_NEXT;
    indirect[1].next = 2;

    indirect[2].addr = vv_virt_to_phys(status);
    indirect[2].len = 1;
    indirect[2].flags = VRING_DESC_F_WRITE;
    indirect[2].next = 0;

    vv_desc(vr, 0, indirect, 3 * 16, VRING_DESC_F_INDIRECT, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0124, VIRTIO_PCI_DEVICE_BLK, test_blk_indirect_read,
              "Read via indirect descriptor table",
              VIRTIO_SPEC_V1_2, "5.2.6");
