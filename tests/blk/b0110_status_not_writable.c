/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0110: Status descriptor not marked writable
 *
 * Spec 5.2.6: The status byte must be device-writable.
 * Mark status descriptor as readable (no WRITE flag).
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_status_not_writable(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    /* Status NOT writable */
    vv_desc(vr, 2, status, 1, 0, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0110, VIRTIO_PCI_DEVICE_BLK, test_blk_status_not_writable,
              "Status descriptor not marked writable",
              VIRTIO_SPEC_V1_2, "5.2.6");
