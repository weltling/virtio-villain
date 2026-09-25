/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0076: desc_all_max_values
 *
 * Packed twin of T0082. Present a descriptor whose addr, len, and id
 * are all at their maximum and whose flags word is 0xFFFF, so every
 * flag bit including AVAIL is set. The device must handle the extreme
 * values without faulting on the impossible address or length.
 */
#include "tests/packed/packed_util.h"

static test_result_t test_packed_desc_all_max_values(struct virtio_dev *dev,
                                                    struct vring_packed *vr)
{
    uint16_t a0 = vr->next_avail;
    uint8_t w0 = vr->wrap_counter;

    vr->desc[a0].addr = 0xFFFFFFFFFFFFFFFFULL;
    vr->desc[a0].len = 0xFFFFFFFF;
    vr->desc[a0].id = 0xFFFF;
    __sync_synchronize();
    /* All flag bits set, which includes the AVAIL phase bit. */
    vr->desc[a0].flags = 0xFFFF;
    vring_packed_advance(vr);

    return vv_kick_and_wait_packed(dev, vr, 0, a0, w0, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0076, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_desc_all_max_values,
                     "Packed descriptor with addr/len/id/flags all at max",
                     VIRTIO_SPEC_V1_2, "2.8.6");
