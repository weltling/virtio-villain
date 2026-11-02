/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0005: desc_addr_beyond_ram
 *
 * Set a descriptor's guest physical address to a value far beyond the
 * VM's RAM size. The VMM must translate GPAs to host virtual addresses
 * for DMA; if it skips bounds validation, the translated pointer is
 * invalid and any access through it causes a SIGSEGV in the VMM process.
 */
#include "lib/virtio_test.h"

static test_result_t test_desc_addr_beyond_ram(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /*
     * Descriptor chain:
     *   [0] valid header (readable)
     *   [1] data buffer at GPA 0xFFFF_FFFF_FFFF_0000 (writable)
     *   [2] status byte (writable, valid)
     *
     * The data buffer GPA is far beyond any reasonable guest RAM.
     * A VMM that does GPA-to-HVA translation without checking the
     * GPA falls within a valid memory slot will segfault.
     */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, 0xFFFFFFFFFFFF0000ULL, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0005, VIRTIO_PCI_DEVICE_BLK, test_desc_addr_beyond_ram,
              "Descriptor GPA beyond guest RAM",
              VIRTIO_SPEC_V1_2, "2.7.5");
