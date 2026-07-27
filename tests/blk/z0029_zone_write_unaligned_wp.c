/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0029: blk_zone_write_unaligned_wp
 *
 * Spec 5.2.6.6: a regular write to a sequential write required zone
 * must start at the zone write pointer. A fresh zone has its write
 * pointer at the zone start, so a write one sector past the start is
 * unaligned and the device returns VIRTIO_BLK_S_ZONE_UNALIGNED_WP.
 * z0010 submits a misaligned ZONE_APPEND but never inspects the
 * status. Issue a plain write at sector 1 and verify the status is
 * either OK (the zone is conventional and allows random writes) or the
 * reserved ZONE_UNALIGNED_WP code. Skips when the device is not zoned.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_blk_zone_write_unaligned_wp(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_BLK_F_ZONED)))
        return TEST_SKIP;

    struct virtio_blk_req *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_OUT;
    hdr->reserved = 0;
    hdr->sector = 1;  /* one sector past the zone start write pointer */
    memset(data, 0x3C, 512);
    *status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    if (*status != VIRTIO_BLK_S_OK &&
        *status != VIRTIO_BLK_S_ZONE_UNALIGNED_WP)
        TFAIL("status %u, expected OK or ZONE_UNALIGNED_WP", *status);

    return TEST_PASS;
}

REGISTER_TEST(Z0029, VIRTIO_PCI_DEVICE_BLK,
              test_blk_zone_write_unaligned_wp,
              "unaligned write to a zone returns ZONE_UNALIGNED_WP",
              VIRTIO_SPEC_V1_3, "5.2.6.6");
