/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0011: ZONE_REPORT request with missing status descriptor
 *
 * Spec 5.2.6.6 (v1.3): "When forming a VIRTIO_BLK_T_ZONE_REPORT
 * request, the driver MUST set a device-writable buffer" for the
 * zone report data and a status byte.
 *
 * Submit ZONE_REPORT with only a header and a data buffer but
 * no status descriptor (incomplete request chain).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_zone_report_no_status(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_blk_req *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_ZONE_REPORT;
    hdr->reserved = 0;
    hdr->sector = 0;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);

    /* Header (device-readable) */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    /* Data buffer (device-writable) but NO status descriptor */
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(Z0011, VIRTIO_PCI_DEVICE_BLK, test_zone_report_no_status,
              "Zone report with no status descriptor (truncated chain)",
              VIRTIO_SPEC_V1_3, "5.2.6.6");
