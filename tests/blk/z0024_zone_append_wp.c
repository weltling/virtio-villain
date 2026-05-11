/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0024: blk_zone_append_at_wp
 *
 * Submit a zone append at sector 0 (start of first zone). The
 * device should accept the write and advance the write pointer.
 * Basic positive path test per spec v1.3 5.2.6.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_BLK_T_ZONE_APPEND 32

struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed));

static test_result_t test_blk_zone_append_wp(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_blk_req *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_ZONE_APPEND;
    hdr->reserved = 0;
    hdr->sector = 0;
    memset(data, 0x55, 512);
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    /* header (device-readable) */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    /* data (device-readable) */
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT, 2);
    /* status (device-writable) */
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(Z0024, VIRTIO_PCI_DEVICE_BLK, test_blk_zone_append_wp,
              "Zone append at write pointer",
              VIRTIO_SPEC_V1_3, "5.2.6");
