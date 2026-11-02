/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0161: blk_get_id_with_readonly_buffer
 *
 * Submit GET_ID but make the data buffer device readable instead
 * of device writable. Spec 5.2.6.1 says the data buffer for
 * GET_ID must be writable by the device. The device must reject
 * the malformed request.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_getid_readonly(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_GET_ID, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    /* Data buffer: NOT writable (missing VRING_DESC_F_WRITE) */
    vv_desc(vr, 1, data, 20, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0161, VIRTIO_PCI_DEVICE_BLK, test_blk_getid_readonly,
              "GET_ID with device readable data buffer",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
