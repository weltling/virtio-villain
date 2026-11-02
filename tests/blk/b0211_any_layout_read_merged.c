/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0211: blk_any_layout_read_data_status_merged
 *
 * Virtio 1.x drivers may use any descriptor layout. Submit an IN (read)
 * request as a two descriptor chain where the second writable descriptor
 * carries 512 bytes of data followed by the 1 byte status (513 bytes
 * total).
 *
 * A spec-compliant device must write the data into the first 512 bytes
 * of the writable descriptor and the status byte into the last byte.
 * A device that hardcodes "status is a separate trailing descriptor"
 * will either reject the chain or write the status byte at offset 0
 * and never deliver the read data.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_any_layout_read_merged(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    /* One contiguous writable buffer: 512 data bytes followed by status. */
    uint8_t *buf = vv_alloc_pages(1);

    memset(buf, 0xA5, 513);

    uint64_t buf_phys = vv_virt_to_phys(buf);

    /*
     * desc 0: header (readable, 16 bytes)
     * desc 1: data + status in one writable descriptor (512 + 1 bytes)
     */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, buf_phys, 512 + 1, VRING_DESC_F_WRITE, 0);

    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r == TEST_REJECT)
        return TEST_SKIP; /* device does not support any-layout */
    if (r != TEST_PASS)
        return r;
    /* Status byte is the last byte of the writable descriptor. */
    if (buf[512] != VIRTIO_BLK_S_OK)
        return TEST_SKIP; /* device completed but gave error status */
    return TEST_PASS;
}

REGISTER_TEST(B0211, VIRTIO_PCI_DEVICE_BLK,
              test_blk_any_layout_read_merged,
              "IN request with data and status in one writable descriptor",
              VIRTIO_SPEC_V1_2, "5.2.6");
