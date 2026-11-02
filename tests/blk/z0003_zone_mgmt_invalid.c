/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0003: blk_zone_mgmt_invalid_id
 *
 * Submit a VIRTIO_BLK_T_ZONE_RESET command with an invalid zone
 * sector (not aligned to zone size) without ZONED feature negotiated.
 * Tests that the device handles invalid zone management commands.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_zone_mgmt_invalid(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_blk_req *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_ZONE_RESET;
    hdr->reserved = 0;
    hdr->sector = 7; /* unlikely to be zone-aligned */
    *status = 0xFF;

    /* header (device-readable) */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    /* status (device-writable) */
    vv_desc(vr, 1, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(Z0003, VIRTIO_PCI_DEVICE_BLK, test_blk_zone_mgmt_invalid,
              "Zone reset with invalid/unaligned sector",
              VIRTIO_SPEC_V1_3, "5.2.6");
