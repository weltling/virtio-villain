/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0001: blk_zoned_no_feature
 *
 * Submit a zoned block device command (VIRTIO_BLK_T_ZONE_REPORT)
 * without negotiating VIRTIO_BLK_F_ZONED. Spec v1.3 5.2: driver
 * MUST NOT negotiate the ZONED feature if incapable, and MUST NOT
 * issue zoned commands without the feature.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

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

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    /* header (device-readable) */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    /* data buffer (device-writable) for zone report */
    vring_raw_set_desc(vr, 1, data_phys, 4096,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    /* status (device-writable) */
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(Z0001, VIRTIO_PCI_DEVICE_BLK, test_blk_zoned_no_feature,
              "Zone report command without ZONED feature",
              VIRTIO_SPEC_V1_3, "5.2.6");
