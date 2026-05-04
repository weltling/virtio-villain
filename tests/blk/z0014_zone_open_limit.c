/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0014: blk_zone_open_limit
 *
 * Attempt to open more zones than max_open_zones allows. Submit
 * ZONE_OPEN commands for many distinct zones. The device should
 * reject opens beyond the limit.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_BLK_T_ZONE_OPEN 24
#define ZONE_SIZE_SECTORS 524288  /* 256 MiB zone at 512 bytes/sector */
#define NUM_OPENS 64              /* try to open 64 zones */

struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed));

static test_result_t test_blk_zone_open_limit(struct virtio_dev *dev,
                                              struct vring *vr)
{
    /*
     * Submit NUM_OPENS zone open requests as separate descriptors
     * in the avail ring. At least some should be rejected if
     * max_open_zones < NUM_OPENS.
     */
    for (int i = 0; i < NUM_OPENS; i++) {
        struct virtio_blk_req *hdr = vv_alloc_pages(1);
        uint8_t *status = vv_alloc_pages(1);

        hdr->type = VIRTIO_BLK_T_ZONE_OPEN;
        hdr->reserved = 0;
        hdr->sector = (uint64_t)i * ZONE_SIZE_SECTORS;
        *status = 0xFF;

        uint16_t base = (uint16_t)(i * 2);
        vring_raw_set_desc(vr, base, vv_virt_to_phys(hdr),
                           sizeof(*hdr), VRING_DESC_F_NEXT, base + 1);
        vring_raw_set_desc(vr, base + 1, vv_virt_to_phys(status), 1,
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, i, base);
    }

    vring_raw_set_avail_idx(vr, NUM_OPENS);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(Z0014, VIRTIO_PCI_DEVICE_BLK, test_blk_zone_open_limit,
              "Open more zones than max_open_zones allows",
              VIRTIO_SPEC_V1_3, "5.2.6.6");
