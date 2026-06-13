/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0022: blk_zone_concurrent_ops
 *
 * Submit two zone management operations concurrently on the same
 * zone (ZONE_OPEN + ZONE_CLOSE). Tests that the device serializes
 * or handles concurrent zone operations correctly.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_zone_concurrent(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_req *hdr1 = vv_alloc_pages(1);
    struct virtio_blk_req *hdr2 = vv_alloc_pages(1);
    uint8_t *status1 = vv_alloc_pages(1);
    uint8_t *status2 = (uint8_t *)status1 + 64;

    hdr1->type = VIRTIO_BLK_T_ZONE_OPEN;
    hdr1->reserved = 0;
    hdr1->sector = 0;

    hdr2->type = VIRTIO_BLK_T_ZONE_CLOSE;
    hdr2->reserved = 0;
    hdr2->sector = 0;

    *status1 = 0xFF;
    *status2 = 0xFF;

    uint64_t hdr1_phys = vv_virt_to_phys(hdr1);
    uint64_t hdr2_phys = vv_virt_to_phys(hdr2);
    uint64_t status1_phys = vv_virt_to_phys(status1);
    uint64_t status2_phys = vv_virt_to_phys(status2);

    /* First request: ZONE_OPEN */
    vring_raw_set_desc(vr, 0, hdr1_phys, sizeof(*hdr1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, status1_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    /* Second request: ZONE_CLOSE */
    vring_raw_set_desc(vr, 2, hdr2_phys, sizeof(*hdr2),
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, status2_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(Z0022, VIRTIO_PCI_DEVICE_BLK, test_blk_zone_concurrent,
              "Concurrent zone open and close on same zone",
              VIRTIO_SPEC_V1_3, "5.2.6");
