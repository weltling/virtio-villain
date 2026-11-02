/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0001: blk_zoned_no_feature
 *
 * Submit a zoned block device command (VIRTIO_BLK_T_ZONE_REPORT)
 * without negotiating VIRTIO_BLK_F_ZONED. Spec v1.3 5.2: driver
 * MUST NOT negotiate the ZONED feature if incapable, and MUST NOT
 * issue zoned commands without the feature.
 */
#include "lib/virtio_test.h"

/* Zoned block request types (v1.3) */

static test_result_t test_blk_zoned_no_feature(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_req *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    /* Zone report command without ZONED feature negotiated */
    hdr->type = VIRTIO_BLK_T_ZONE_REPORT;
    hdr->reserved = 0;
    hdr->sector = 0;

    /* header (device-readable) */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    /* data buffer (device-writable) for zone report */
    vv_desc(vr, 1, data, 4096, VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    /* status (device-writable) */
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(Z0001, VIRTIO_PCI_DEVICE_BLK, test_blk_zoned_no_feature,
              "Zone report command without ZONED feature",
              VIRTIO_SPEC_V1_3, "5.2.6");
