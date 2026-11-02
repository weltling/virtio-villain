/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0006: desc_len_zero
 *
 * Submit a request where the data descriptor has len = 0. Some VMMs
 * may not handle zero-length transfers, leading to division by zero
 * or assertion failures when computing sector counts.
 */
#include "lib/virtio_test.h"

static test_result_t test_desc_len_zero(struct virtio_dev *dev,
                                        struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);

    /*
     * Descriptor chain:
     *   [0] header (readable)
     *   [1] data buffer with len = 0 (writable)
     *   [2] status byte (writable)
     */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, hdr_phys, 0,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0006, VIRTIO_PCI_DEVICE_BLK, test_desc_len_zero,
              "Data descriptor with len = 0",
              VIRTIO_SPEC_V1_2, "2.7.5");
