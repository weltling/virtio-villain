/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0004: desc_addr_zero
 *
 * Set a data descriptor's guest physical address to 0 (NULL). Many VMMs
 * map GPA 0 to something valid internally but some treat it as an error.
 * A VMM that blindly translates GPA 0 to a host pointer and writes to
 * it may corrupt page zero or trigger a null-pointer dereference if the
 * host has mmap_min_addr protections.
 */
#include "lib/virtio_test.h"

static test_result_t test_desc_addr_zero(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /*
     * Descriptor chain:
     *   [0] valid header (readable)
     *   [1] data buffer at GPA 0x0 (writable)
     *   [2] status byte (writable, valid)
     */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, 0x0ULL, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0004, VIRTIO_PCI_DEVICE_BLK, test_desc_addr_zero,
              "Descriptor with NULL guest physical address",
              VIRTIO_SPEC_V1_2, "2.7.5");
