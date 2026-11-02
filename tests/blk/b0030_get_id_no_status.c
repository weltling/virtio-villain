/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0030: blk_get_id_no_status
 *
 * Submit a GET_ID request with a correct 20-byte data buffer but with
 * no status descriptor. The device needs the status byte to report
 * success/failure; without it the chain is malformed.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_get_id_no_status(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_GET_ID, 0);
    uint8_t *data = vv_alloc_pages(1);

    uint64_t hdr_phys = vv_virt_to_phys(hdr);

    /* Header (readable) -> 20-byte data (writable) but NO status desc */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 20, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0030, VIRTIO_PCI_DEVICE_BLK, test_blk_get_id_no_status,
              "GET_ID with 20-byte data but missing status descriptor",
              VIRTIO_SPEC_V1_2, "5.2.6");
