/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0088: Discard segment with sector = capacity (beyond valid range)
 *
 * Spec 5.2.6.1: "A driver MUST NOT submit a request which would
 * cause a read or write beyond capacity."
 *
 * Submit DISCARD where segment.sector equals device capacity.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_discard_at_capacity(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    /* Read capacity from device config */
    uint64_t capacity = 0;
    volatile uint8_t *cfg_space = dev->device_cfg;
    for (int i = 0; i < 8; i++)
        ((uint8_t *)&capacity)[i] = cfg_space[i];

    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_DISCARD, 0);
    struct virtio_blk_discard_write_zeroes *seg = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    seg->sector = capacity; /* at capacity = beyond valid */
    seg->num_sectors = 1;
    seg->flags = 0;

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, seg, sizeof(*seg), VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(B0088, VIRTIO_PCI_DEVICE_BLK, test_blk_discard_at_capacity,
              "Discard segment with sector == capacity (beyond device)",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
