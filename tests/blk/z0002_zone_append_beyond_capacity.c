/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0002: blk_zone_append_beyond_capacity
 *
 * Submit a VIRTIO_BLK_T_ZONE_APPEND command targeting a sector beyond
 * the device capacity, without negotiating VIRTIO_BLK_F_ZONED.
 * Tests that the device rejects out-of-bounds zone appends.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_zone_append_oob(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_req *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_ZONE_APPEND;
    hdr->reserved = 0;
    hdr->sector = 0xFFFFFFFFFFFFFFFFULL; /* way beyond capacity */
    memset(data, 0xAA, 512);
    *status = 0xFF;

    /* header (device-readable) */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    /* data to append (device-readable) */
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT, 2);
    /* status (device-writable) */
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(Z0002, VIRTIO_PCI_DEVICE_BLK, test_blk_zone_append_oob,
              "Zone append targeting sector beyond capacity",
              VIRTIO_SPEC_V1_3, "5.2.6");
