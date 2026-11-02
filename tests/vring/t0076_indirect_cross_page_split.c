/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0076: indirect_table_at_page_boundary
 *
 * Place an indirect descriptor table so that it straddles two pages.
 * The table starts near the end of one page and extends into the next.
 * Tests device handling of non-contiguous physical backing for the
 * indirect table (identity-mapped here, but stresses page logic).
 */
#include "lib/virtio_test.h"

static test_result_t test_indirect_cross_page_split(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /*
     * Allocate 2 contiguous pages, then place the indirect table
     * starting at offset 4080 (16 bytes before page boundary).
     * Each desc is 16 bytes, so desc[0] is in page 1, desc[1+] in page 2.
     */
    uint8_t *pages = vv_alloc_pages(2);
    struct vring_desc *indirect = (struct vring_desc *)(pages + 4096 - 16);

    /* 3 descriptors = 48 bytes, crossing the page boundary */
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

    uint64_t indirect_phys = vv_virt_to_phys(indirect);

    vring_raw_set_desc(vr, 0, indirect_phys, 3 * sizeof(struct vring_desc),
                       VRING_DESC_F_INDIRECT, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0076, VIRTIO_PCI_DEVICE_BLK, test_indirect_cross_page_split,
              "Indirect table straddling page boundary",
              VIRTIO_SPEC_V1_2, "2.7.5.3");
