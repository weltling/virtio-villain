/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0010: zone append with non-512-aligned sector
 *
 * Spec 5.2.6.6 (v1.3): "A zone sector address provided by the
 * driver MUST be a multiple of 512 bytes."
 *
 * Submit ZONE_APPEND with sector = 1 (512 bytes from start, but
 * not zone-aligned; the zone start must be zone-size-aligned).
 * Actually the spec says sector address must be multiple of 512,
 * which in sector units means any value is fine. The real constraint
 * is that sector must equal the start of a zone. Use a sector value
 * that is clearly not a zone start (odd value like 3).
 */
#include "lib/virtio_test.h"

static test_result_t test_zone_append_misaligned(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_blk_req *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_ZONE_APPEND;
    hdr->reserved = 0;
    /* Sector 3 — not zone-aligned (zones start at multiples of zone size) */
    hdr->sector = 3;
    memset(data, 0xBB, 512);
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(Z0010, VIRTIO_PCI_DEVICE_BLK, test_zone_append_misaligned,
              "Zone append with sector not at zone start boundary",
              VIRTIO_SPEC_V1_3, "5.2.6.6");
