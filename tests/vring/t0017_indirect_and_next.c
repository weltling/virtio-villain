/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0017: indirect_and_next
 *
 * Set both VRING_DESC_F_INDIRECT and VRING_DESC_F_NEXT on the same
 * descriptor. The spec says "A driver MUST NOT set both INDIRECT and
 * NEXT in flags."
 */
#include "lib/virtio_test.h"

static test_result_t test_indirect_and_next(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);
    struct vring_desc *indirect = vv_alloc_pages(1);

    *status = 0xFF;

    uint64_t status_phys = vv_virt_to_phys(status);
    uint64_t indirect_phys = vv_virt_to_phys(indirect);

    /* Valid indirect table */
    indirect[0].addr = vv_virt_to_phys(hdr);
    indirect[0].len = sizeof(*hdr);
    indirect[0].flags = VRING_DESC_F_NEXT;
    indirect[0].next = 1;
    indirect[1].addr = vv_virt_to_phys(data);
    indirect[1].len = 512;
    indirect[1].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
    indirect[1].next = 2;
    indirect[2].addr = status_phys;
    indirect[2].len = 1;
    indirect[2].flags = VRING_DESC_F_WRITE;
    indirect[2].next = 0;

    /*
     * Main descriptor: both INDIRECT and NEXT set (illegal).
     * next points to descriptor 1 which is unused filler.
     */
    vring_raw_set_desc(vr, 0, indirect_phys, 3 * sizeof(struct vring_desc),
                       VRING_DESC_F_INDIRECT | VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0017, VIRTIO_PCI_DEVICE_BLK, test_indirect_and_next,
              "Both INDIRECT and NEXT flags set",
              VIRTIO_SPEC_V1_2, "2.7.5.3");
