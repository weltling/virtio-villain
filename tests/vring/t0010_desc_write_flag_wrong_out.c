/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0010: desc_write_flag_wrong_data_out
 *
 * Submit a virtio-blk OUT (write) request where the data buffer is
 * marked WRITE (device-writable). For an OUT request the data must be
 * device-readable so the device can read from it. A VMM that ignores
 * the mismatch may read from a buffer it was supposed to write, or
 * corrupt guest memory.
 */
#include "lib/virtio_test.h"

static test_result_t test_desc_write_flag_wrong_out(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_OUT, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    memset(data, 0x42, 512);
    *status = 0xFF;

    /*
     * Descriptor chain:
     *   [0] header (readable) - correct
     *   [1] data (WRITE!) - WRONG for OUT, should be readable
     *   [2] status (writable) - correct
     */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0010, VIRTIO_PCI_DEVICE_BLK, test_desc_write_flag_wrong_out,
              "Data descriptor WRITE flag set on OUT request",
              VIRTIO_SPEC_V1_2, "2.7.5");
