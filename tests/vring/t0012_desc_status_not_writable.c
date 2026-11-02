/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0012: desc_status_not_writable
 *
 * Submit a request where the status descriptor lacks the WRITE flag.
 * The device writes the completion status byte into this descriptor;
 * without WRITE it should be rejected.
 */
#include "lib/virtio_test.h"

static test_result_t test_desc_status_not_writable(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /*
     * Descriptor chain:
     *   [0] header (readable) - correct
     *   [1] data (writable) - correct
     *   [2] status (readable!) - WRONG, device writes status here
     */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       0, 0); /* no WRITE flag */

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0012, VIRTIO_PCI_DEVICE_BLK, test_desc_status_not_writable,
              "Status descriptor without WRITE flag",
              VIRTIO_SPEC_V1_2, "5.2.6");
