/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0025: Zone append to a full zone.
 *
 * Spec v1.3 5.2.6: Submit a ZONE_APPEND targeting the first zone
 * after issuing enough appends to fill it (simulated by using a
 * sector far past the zone's capacity). The device should reject
 * the operation rather than writing past the zone boundary.
 * Here we use sector = 0xFFFFFFFF to approximate a full condition.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_zone_append_full(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_req *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_ZONE_APPEND;
    hdr->reserved = 0;
    hdr->sector = 0xFFFFFFFFULL;
    memset(data, 0x55, 512);
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(Z0025, VIRTIO_PCI_DEVICE_BLK, test_blk_zone_append_full,
              "Zone append at invalid sector past zone capacity",
              VIRTIO_SPEC_V1_3, "5.2.6");
