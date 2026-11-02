/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0037: blk_oversized_status_descriptor
 *
 * Submit a READ request with a status descriptor that is 512 bytes
 * (far larger than the 1-byte status field). The device should only
 * write 1 byte and not overflow into adjacent memory.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_oversized_status(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    memset(status, 0xCC, 512); /* canary pattern */

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    /* Status descriptor is 512 bytes - way more than needed */
    vv_desc(vr, 2, status, 512, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0037, VIRTIO_PCI_DEVICE_BLK, test_blk_oversized_status,
              "Status descriptor oversized (512 bytes for 1-byte status)",
              VIRTIO_SPEC_V1_2, "5.2.6");
