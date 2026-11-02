/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0092: Write with ioprio set to maximum value
 *
 * Spec 5.2.6: ioprio is defined but typically must be 0
 * unless VIRTIO_BLK_F_RO etc. Submit with ioprio = UINT32_MAX.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_write_ioprio_max(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_OUT, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->ioprio = UINT32_MAX;
    memset(data, 0xAA, 512);
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0092, VIRTIO_PCI_DEVICE_BLK, test_blk_write_ioprio_max,
              "Write with ioprio == UINT32_MAX",
              VIRTIO_SPEC_V1_2, "5.2.6");
