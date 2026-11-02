/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0013: blk_zone_append_wrong_zone
 *
 * Zone append targeting a zone that is not in OPEN state.
 * The device must reject the append since the zone has not
 * been explicitly opened.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_zone_append_wrong(struct virtio_dev *dev,
                                                struct vring *vr)
{
    /*
     * Target a zone far from sector 0 that is unlikely to be open.
     * Use sector 1048576 (assuming 256K-sector zones, this is zone 4).
     */
    struct virtio_blk_req *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_ZONE_APPEND;
    hdr->reserved = 0;
    hdr->sector = 1048576;

    memset(data, 0xCC, 512);
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(Z0013, VIRTIO_PCI_DEVICE_BLK, test_blk_zone_append_wrong,
              "ZONE_APPEND to zone not in OPEN state",
              VIRTIO_SPEC_V1_3, "5.2.6.6");
