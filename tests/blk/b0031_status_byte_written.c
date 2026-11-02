/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0031: blk_status_byte_written
 *
 * Submit a valid read request with the status byte pre-filled to 0xFF.
 * Verify the device actually writes the status byte (changes it from
 * 0xFF). A device that completes the used ring but never writes status
 * leaves the driver unable to determine success/failure.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_status_byte(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    VV_TRY(vv_submit_and_wait(dev, vr, 0));

    /* Device completed - check if status was actually written */
    if (*status == 0xFF)
        TFAIL("*status == 0xFF");

    return TEST_PASS;
}

REGISTER_TEST(B0031, VIRTIO_PCI_DEVICE_BLK, test_blk_status_byte,
              "Verify device writes status byte on completion",
              VIRTIO_SPEC_V1_2, "5.2.6");
