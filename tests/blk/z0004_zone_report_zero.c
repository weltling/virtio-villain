/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0004: blk_zone_report_size_zero
 *
 * Submit a ZONE_REPORT command with report_size (nr_zones) set to 0.
 * Tests device handling of a zero-length zone report request.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

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
    memset(report, 0, sizeof(*report));

    *status = 0xFF;

    /* header (readable) */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    /* report header (writable) - only room for the header, 0 zone entries */
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(report), sizeof(*report),
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    /* status */
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(Z0004, VIRTIO_PCI_DEVICE_BLK, test_blk_zone_report_zero,
              "ZONE_REPORT with zero-size report buffer",
              VIRTIO_SPEC_V1_3, "5.2.6");
