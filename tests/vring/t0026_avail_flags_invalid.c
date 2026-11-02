/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0026: avail_flags_invalid
 *
 * Set avail.flags to 0xFFFF (all bits set, including undefined ones).
 * The only defined bit is VRING_AVAIL_F_NO_INTERRUPT (bit 0). A VMM
 * must ignore unknown flag bits.
 */
#include "lib/virtio_test.h"

static test_result_t test_avail_flags_invalid(struct virtio_dev *dev,
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

    /* Set all avail flag bits */
    vr->avail->flags = 0xFFFF;
    __sync_synchronize();

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0026, VIRTIO_PCI_DEVICE_BLK, test_avail_flags_invalid,
              "avail.flags with undefined bits set",
              VIRTIO_SPEC_V1_2, "2.7.6");
