/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0034: blk_write_zero_length_data
 *
 * Submit a WRITE request with a zero-length data descriptor. The device
 * must handle writing zero bytes gracefully without corrupting internal
 * state or crashing.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_write_zero_length(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_OUT, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t status_phys = vv_virt_to_phys(status);

    /* Header (readable) -> data with len=0 (readable) -> status (writable) */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 0, VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0034, VIRTIO_PCI_DEVICE_BLK, test_blk_write_zero_length,
              "WRITE request with zero-length data descriptor",
              VIRTIO_SPEC_V1_2, "5.2.6");
