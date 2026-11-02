/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0028: used_ring_guest_write
 *
 * The guest writes to the used ring (which is owned by the device).
 * Set used.idx and used.ring[0] to bogus values before submitting a
 * request. A correct VMM ignores guest writes to device-owned memory.
 */
#include "lib/virtio_test.h"

static test_result_t test_used_ring_guest_write(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /* Valid descriptor chain */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    /*
     * Corrupt the used ring before submitting. The device should
     * overwrite these with correct values.
     */
    vr->used->idx = 0xBEEF;
    vr->used->ring[0].id = 0xDEAD;
    vr->used->ring[0].len = 0xCAFE;
    __sync_synchronize();

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0028, VIRTIO_PCI_DEVICE_BLK, test_used_ring_guest_write,
              "Guest writes to used ring (device-owned)",
              VIRTIO_SPEC_V1_2, "2.7.8");
