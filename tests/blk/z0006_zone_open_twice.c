/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0006: blk_zone_open_already_open
 *
 * Submit ZONE_OPEN twice for the same zone (sector 0). Tests device
 * handling of idempotent zone management - opening an already-open
 * zone should succeed or be a no-op, not cause errors.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_BLK_T_ZONE_OPEN 24

struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed));

static test_result_t test_blk_zone_open_twice(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_req *hdr1 = vv_alloc_pages(1);
    uint8_t *status1 = vv_alloc_pages(1);
    struct virtio_blk_req *hdr2 = vv_alloc_pages(1);
    uint8_t *status2 = vv_alloc_pages(1);

    hdr1->type = VIRTIO_BLK_T_ZONE_OPEN;
    hdr1->reserved = 0;
    hdr1->sector = 0;
    *status1 = 0xFF;

    hdr2->type = VIRTIO_BLK_T_ZONE_OPEN;
    hdr2->reserved = 0;
    hdr2->sector = 0; /* same zone again */
    *status2 = 0xFF;

    /* First OPEN: descs 0-1 */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr1), sizeof(*hdr1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(status1), 1,
                       VRING_DESC_F_WRITE, 0);

    /* Second OPEN: descs 2-3 */
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(hdr2), sizeof(*hdr2),
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(status2), 1,
                       VRING_DESC_F_WRITE, 0);

    /* Both in avail ring */
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(Z0006, VIRTIO_PCI_DEVICE_BLK, test_blk_zone_open_twice,
              "ZONE_OPEN same zone twice (idempotent check)",
              VIRTIO_SPEC_V1_3, "5.2.6");
