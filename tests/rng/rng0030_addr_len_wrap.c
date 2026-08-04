/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0030: writable descriptor whose addr plus len wraps past 2^64.
 *
 * Siblings RNG0004 and RNG0029 cover a valid base with a tail past the
 * top of System RAM and a base sitting exactly at the top of RAM. Both
 * have an end address that stays below 2^64. Here the base sits near the
 * top of the address space and the length makes addr plus len wrap to a
 * low value, so a device that computes the end with a plain addition
 * gets a small wrapped result. A naive bounds check that compares the
 * wrapped end against RAM size passes, and the device may then access a
 * wild range. This is the entropy write path, which does not go through
 * the block paged range builder, so it exercises the generic guest
 * memory access on its own.
 *
 * The device must not access memory outside the guest mapping or crash
 * the VMM. Completing, silently rejecting, or wedging the queue are all
 * acceptable. Triple faulting the guest or crashing the VMM is not.
 *
 * Spec 2.7.5.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

static test_result_t test_rng_addr_len_wrap(struct virtio_dev *dev,
                                            struct vring *vr)
{
    /*
     * base = 2^64 - 4096, len = 0x2000 -> end wraps to 0x1000.
     */
    vring_raw_set_desc(vr, 0, 0xFFFFFFFFFFFFF000ULL, 0x2000,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RNG0030, VIRTIO_PCI_DEVICE_RNG, test_rng_addr_len_wrap,
              "RNG writable descriptor addr plus len wraps 64 bits",
              VIRTIO_SPEC_V1_2, "2.7.5");
