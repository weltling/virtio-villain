/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0007: blk_zone_finish_empty
 *
 * Submit ZONE_FINISH for a zone that has never been written to
 * (still empty). Tests device handling of finishing a zone with
 * no data - should it succeed as a transition or fail?
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_zone_finish_empty(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_blk_req *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_ZONE_FINISH;
    hdr->reserved = 0;
    hdr->sector = 0; /* first zone, presumably empty */
    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(Z0007, VIRTIO_PCI_DEVICE_BLK, test_blk_zone_finish_empty,
              "ZONE_FINISH on empty (never-written) zone",
              VIRTIO_SPEC_V1_3, "5.2.6");
