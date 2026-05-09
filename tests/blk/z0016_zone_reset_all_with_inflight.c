/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0016: zone_reset_all_with_inflight
 *
 * Queue a ZONE_APPEND chain in slot 0 and a ZONE_MGMT RESET ALL
 * chain in slot 1, place both in the avail ring before the kick.
 * Spec v1.3 5.2.6 requires the device to consume both entries
 * deterministically rather than dropping one or wedging.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_ZONE_APPEND 15
#define VIRTIO_BLK_T_ZONE_MGMT   16
#define ZONE_MGMT_RESET_ALL      0x05

static test_result_t test_zone_reset_all_inflight(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_blk_outhdr *h1 = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st1 = vv_alloc_pages(1);
    struct virtio_blk_outhdr *h2 = vv_alloc_pages(1);
    uint8_t *st2 = vv_alloc_pages(1);

    h1->type = VIRTIO_BLK_T_ZONE_APPEND;
    h1->ioprio = 0;
    h1->sector = 0;
    *st1 = 0xFF;

    h2->type = VIRTIO_BLK_T_ZONE_MGMT;
    h2->ioprio = ZONE_MGMT_RESET_ALL;
    h2->sector = 0;
    *st2 = 0xFF;

    /* Chain 1 */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(h1), sizeof(*h1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st1), 1,
                       VRING_DESC_F_WRITE, 0);

    /* Chain 2 */
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(h2), sizeof(*h2),
                       VRING_DESC_F_NEXT, 4);
    vring_raw_set_desc(vr, 4, vv_virt_to_phys(st2), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 3);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(Z0016, VIRTIO_PCI_DEVICE_BLK, test_zone_reset_all_inflight,
              "ZONE_RESET_ALL with a queued ZONE_APPEND",
              VIRTIO_SPEC_V1_3, "5.2.6");
