/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0004: blk_zone_report_size_zero
 *
 * Submit a ZONE_REPORT command with report_size (nr_zones) set to 0.
 * Tests device handling of a zero-length zone report request.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_zone_report_zero(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_req *hdr = vv_alloc_pages(1);
    struct virtio_blk_zone_report_hdr *report = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_ZONE_REPORT;
    hdr->reserved = 0;
    hdr->sector = 0;

    /* Request 0 zones in the report */

    *status = 0xFF;

    /* header (readable) */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    /* report header (writable) - only room for the header, 0 zone entries */
    vv_desc(vr, 1, report, sizeof(*report),
            VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    /* status */
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(Z0004, VIRTIO_PCI_DEVICE_BLK, test_blk_zone_report_zero,
              "ZONE_REPORT with zero-size report buffer",
              VIRTIO_SPEC_V1_3, "5.2.6");
