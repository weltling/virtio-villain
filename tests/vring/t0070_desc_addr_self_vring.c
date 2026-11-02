/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0070: desc_addr_self_vring
 *
 * Set a descriptor address pointing into the virtqueue's own memory
 * region (desc table, avail ring, or used ring). If the device
 * writes DMA data into its own queue structures, it may corrupt
 * subsequent descriptor processing or create infinite loops.
 */
#include "lib/virtio_test.h"

static test_result_t test_desc_addr_self_vring(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /*
     * Point the data descriptor at the descriptor table itself.
     * A 512-byte DMA write here would overwrite desc entries.
     */
    uint64_t desc_table_phys = vv_virt_to_phys(vr->desc);

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, desc_table_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0070, VIRTIO_PCI_DEVICE_BLK, test_desc_addr_self_vring,
              "Descriptor address pointing into own virtqueue memory",
              VIRTIO_SPEC_V1_2, "2.7.5");
