/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0003: blk_zone_mgmt_invalid_id
 *
 * Submit a VIRTIO_BLK_T_ZONE_RESET command with an invalid zone
 * sector (not aligned to zone size) without ZONED feature negotiated.
 * Tests that the device handles invalid zone management commands.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_BLK_T_ZONE_RESET 30

struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed));

static test_result_t test_blk_zone_mgmt_invalid(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_blk_req *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_ZONE_RESET;
    hdr->reserved = 0;
    hdr->sector = 7; /* unlikely to be zone-aligned */
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t status_phys = vv_virt_to_phys(status);

    /* header (device-readable) */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    /* status (device-writable) */
    vring_raw_set_desc(vr, 1, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(Z0003, VIRTIO_PCI_DEVICE_BLK, test_blk_zone_mgmt_invalid,
              "Zone reset with invalid/unaligned sector",
              VIRTIO_SPEC_V1_3, "5.2.6");
