/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0064: indirect_too_many_entries
 *
 * Create an indirect descriptor table with more entries than queue_size.
 * The spec says the table length must not exceed queue_size entries.
 * Tests that the device enforces this limit.
 */
#include "lib/virtio_test.h"

static test_result_t test_indirect_too_many(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /*
     * Build indirect table with 128 entries (queue_size is 64).
     * Only the first 3 are meaningful, rest are padding.
     */
    struct vring_desc *indirect = vv_alloc_pages(4);
    uint16_t num_entries = 128; /* > queue_size */

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

    /* Fill remaining with zeros */
    memset(&indirect[3], 0, (num_entries - 3) * sizeof(struct vring_desc));

    /* Point ring descriptor at indirect table with oversized len */
    vv_desc(vr, 0, indirect, num_entries * sizeof(struct vring_desc),
            VRING_DESC_F_INDIRECT, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0064, VIRTIO_PCI_DEVICE_BLK, test_indirect_too_many,
              "Indirect table with more entries than queue_size",
              VIRTIO_SPEC_V1_2, "2.7.5.3");
