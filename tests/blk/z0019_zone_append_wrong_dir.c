/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0019: blk_zone_append_wrong_direction
 *
 * Submit a zone append with data directed to the wrong descriptor
 * direction (device readable in status position). The device must
 * detect the protocol violation.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_zone_append_bad_dir(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_blk_req *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_ZONE_APPEND;
    hdr->reserved = 0;
    hdr->sector = 0;
    memset(data, 0xAA, 512);
    *status = 0xFF;

    /* header (device-readable) */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    /* data: should be device-readable for append, but mark writable */
    vv_desc(vr, 1, data, 512, VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    /* status (device-writable) */
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(Z0019, VIRTIO_PCI_DEVICE_BLK, test_blk_zone_append_bad_dir,
              "Zone append with wrong descriptor direction",
              VIRTIO_SPEC_V1_3, "5.2.6");
