/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0020: blk_zone_report_num_zones_exceed
 *
 * Submit a zone report requesting more zones than exist on the device.
 * Spec v1.3 5.2.6: device must clamp the response to the actual
 * number of zones.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_zone_report_exceed(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_blk_req *hdr = vv_alloc_pages(1);
    uint8_t *result = vv_alloc_pages(2);
    uint8_t *status = (uint8_t *)result + 4096;

    hdr->type = VIRTIO_BLK_T_ZONE_REPORT;
    hdr->reserved = 0;
    hdr->sector = 0;
    *status = 0xFF;

    /* Request an absurdly large number of zones via num_zones field.
     * The data length limits how many can be returned. */
    uint32_t *nr_zones = (uint32_t *)((uint8_t *)hdr + sizeof(*hdr));
    *nr_zones = 0xFFFFFFFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t result_phys = vv_virt_to_phys(result);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr) + 4,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, result_phys, 4096,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(Z0020, VIRTIO_PCI_DEVICE_BLK, test_blk_zone_report_exceed,
              "Zone report with num_zones exceeding actual count",
              VIRTIO_SPEC_V1_3, "5.2.6");
