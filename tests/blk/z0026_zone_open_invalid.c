/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0026: Zone OPEN on an invalid zone.
 *
 * Spec v1.3 5.2.6: Submit a ZONE_OPEN targeting a sector that is
 * not the start of any zone. The device must reject it rather than
 * corrupting zone metadata.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_zone_open_invalid(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_blk_req *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_ZONE_OPEN;
    hdr->reserved = 0;
    /* Sector 1 is unlikely to be a zone start */
    hdr->sector = 1;
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(Z0026, VIRTIO_PCI_DEVICE_BLK, test_blk_zone_open_invalid,
              "Zone OPEN at non zone start sector",
              VIRTIO_SPEC_V1_3, "5.2.6");
