/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0015: indirect_without_feature
 *
 * Set VRING_DESC_F_INDIRECT on a descriptor without having negotiated
 * VIRTIO_F_INDIRECT_DESC. The spec says the driver MUST NOT set this
 * flag unless the feature was negotiated.
 */
#include "lib/virtio_test.h"

static test_result_t test_indirect_without_feature(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    /* Build an indirect table on a separate page */
    struct vring_desc *indirect = vv_alloc_pages(1);

    *status = 0xFF;

    uint64_t indirect_phys = vv_virt_to_phys(indirect);

    /* Fill indirect table with a valid 3-descriptor chain */
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
     * Main descriptor: INDIRECT flag set (feature NOT negotiated).
     * Points to the indirect table.
     */
    vring_raw_set_desc(vr, 0, indirect_phys, 3 * sizeof(struct vring_desc),
                       VRING_DESC_F_INDIRECT, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0015, VIRTIO_PCI_DEVICE_BLK, test_indirect_without_feature,
              "INDIRECT flag without VIRTIO_F_INDIRECT_DESC",
              VIRTIO_SPEC_V1_2, "2.7.5.3");
