/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0082: indirect_addr_oob
 *
 * A packed indirect descriptor whose table address is far beyond guest
 * RAM. The device must bounds check the indirect table pointer before
 * reading it; a device that translates the address blindly faults when
 * it fetches the table.
 */
#include "tests/packed/packed_util.h"

static test_result_t test_packed_indirect_addr_oob(struct virtio_dev *dev,
                                                  struct vring_packed *vr)
{
    uint16_t a0 = vr->next_avail;
    uint8_t w0 = vr->wrap_counter;

    pk_append(vr, 0xFFFFFFFFFFFF0000ULL,
              sizeof(struct vring_packed_desc), a0,
              VRING_PACKED_DESC_F_INDIRECT);

    return vv_kick_and_wait_packed(dev, vr, 0, a0, w0, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0082, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_indirect_addr_oob,
                     "Packed indirect table address beyond guest RAM",
                     VIRTIO_SPEC_V1_2, "2.8.6");
