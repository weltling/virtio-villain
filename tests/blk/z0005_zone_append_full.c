/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0005: blk_zone_append_full
 *
 * Submit a ZONE_APPEND to sector 0 with data. If the zone is full
 * the device should return an error status. Tests device handling
 * of append operations without ZONED feature negotiation.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_zone_append_full(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_req *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_ZONE_APPEND;
    hdr->reserved = 0;
    hdr->sector = 0; /* first zone */

    memset(data, 0xAA, 512);
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(Z0005, VIRTIO_PCI_DEVICE_BLK, test_blk_zone_append_full,
              "ZONE_APPEND without zoned feature (or to full zone)",
              VIRTIO_SPEC_V1_3, "5.2.6");
