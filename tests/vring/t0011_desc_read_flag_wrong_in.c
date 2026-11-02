/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0011: desc_read_flag_wrong_data_in
 *
 * Submit a virtio-blk IN (read) request where the data buffer is
 * device-readable (no WRITE flag). For an IN request the device needs
 * to write data into the buffer, so it must be writable. This is
 * the same class of bug as T09 but explicitly tests the data segment.
 */
#include "lib/virtio_test.h"

static test_result_t test_desc_read_flag_wrong_in(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /*
     * Descriptor chain:
     *   [0] header (readable) - correct
     *   [1] data (readable!) - WRONG for IN, should be writable
     *   [2] status (writable) - correct
     */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT, 2); /* missing WRITE */
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0011, VIRTIO_PCI_DEVICE_BLK, test_desc_read_flag_wrong_in,
              "Data descriptor missing WRITE flag on IN request",
              VIRTIO_SPEC_V1_2, "2.7.5");
