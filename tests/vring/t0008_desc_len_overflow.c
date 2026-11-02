/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0008: desc_len_overflow
 *
 * Set a descriptor's addr to a value near u64 MAX and len large enough
 * that addr + len wraps around the 64 bit address space. A VMM that
 * computes the region end with wrapping addition may pass its own bounds
 * check (the wrapped result is small) and then attempt to access
 * wildly out-of-bounds guest memory.
 *
 * Correct behavior: detect the overflow and reject the descriptor with
 * an I/O error or NEEDS_RESET.
 */
#include "lib/virtio_test.h"

/* virtio-blk request header */
static test_result_t test_desc_len_overflow(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /*
     * Descriptor chain:
     *   [0] header (readable, valid)
     *   [1] data buffer with addr + len overflow (writable)
     *   [2] status byte (writable)
     *
     * desc[1].addr = 0xFFFFFFFF_FFFFFF00, len = 0x200 (512)
     * addr + len = 0x100 (wraps to a tiny address)
     */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, 0xFFFFFFFFFFFFFF00ULL, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    /* Submit head 0 */
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0008, VIRTIO_PCI_DEVICE_BLK, test_desc_len_overflow,
              "Descriptor addr + len wraps 64-bit",
              VIRTIO_SPEC_V1_2, "2.7.5");
