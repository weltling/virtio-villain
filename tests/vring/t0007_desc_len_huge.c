/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0007: desc_len_huge
 *
 * Set a data descriptor's length to 0xFFFFFFFF (4 GiB minus one). Even
 * though the GPA is valid, the length exceeds both the guest RAM and
 * the block device size. A VMM that allocates a bounce buffer based on
 * desc.len, or attempts to perform a 4 GiB DMA transfer, may OOM,
 * overflow a size calculation, or write far beyond the intended buffer.
 */
#include "lib/virtio_test.h"

static test_result_t test_desc_len_huge(struct virtio_dev *dev,
                                        struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    uint64_t data_phys;
    vv_alloc_page_high(&data_phys);

    /*
     * Descriptor chain:
     *   [0] valid header (readable)
     *   [1] data at valid GPA but len = 0xFFFFFFFF (writable)
     *   [2] status byte (writable)
     *
     * The address is valid but the claimed length extends way beyond
     * the actual mapped memory at that GPA.
     */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 0xFFFFFFFF,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0007, VIRTIO_PCI_DEVICE_BLK, test_desc_len_huge,
              "Descriptor with 4 GiB length",
              VIRTIO_SPEC_V1_2, "2.7.5");
