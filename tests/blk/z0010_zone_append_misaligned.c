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
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

#define VIRTIO_BLK_T_ZONE_APPEND 28

struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed));

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

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(Z0010, VIRTIO_PCI_DEVICE_BLK, test_zone_append_misaligned,
              "Zone append with sector not at zone start boundary",
              VIRTIO_SPEC_V1_3, "5.2.6.6");
