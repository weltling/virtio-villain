/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0081: Write request with max sector (UINT64_MAX)
 *
 * Spec 5.2.6.1: "A driver MUST NOT submit a request which would
 * cause a read or write beyond capacity."
 *
 * Submit a write at sector = UINT64_MAX. This is astronomically
 * beyond any device capacity.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_write_max_sector(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_OUT, UINT64_MAX);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    memset(data, 0xEE, 512);
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0081, VIRTIO_PCI_DEVICE_BLK, test_blk_write_max_sector,
              "Write at sector = UINT64_MAX",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
