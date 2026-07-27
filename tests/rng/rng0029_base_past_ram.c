/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0029: writable descriptor whose base sits past the end of RAM.
 *
 * Sibling to RNG0004. There the base is a valid in-range GPA and only
 * the tail runs past the top of System RAM, so the device has to
 * validate addr + len. Here the base itself is the first byte past the
 * top of System RAM, so the whole descriptor is out of range and the
 * device must reject it on the base alone, before any length
 * arithmetic. Unlike RNG0005, which points at a wildly bogus address,
 * this base is adjacent to the real boundary and exercises an
 * off-by-one at ram_top.
 *
 * The device must not access memory outside the guest's mapping or
 * crash the VMM. Completing, silently rejecting, or wedging the queue
 * are all acceptable. Triple faulting the guest or crashing the VMM is
 * not.
 *
 * Spec 2.7.5.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static test_result_t test_rng_base_past_ram(struct virtio_dev *dev,
                                            struct vring *vr)
{
    uint64_t ram_top = vv_parse_ram_top();
    if (ram_top == 0)
        return TEST_SKIP;

    /*
     * The base is the first byte past the top of System RAM, so the
     * entire descriptor lies outside the guest's mapping.
     */
    vring_raw_set_desc(vr, 0, ram_top, PAGE_SIZE, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RNG0029, VIRTIO_PCI_DEVICE_RNG, test_rng_base_past_ram,
              "RNG writable descriptor base past end of RAM",
              VIRTIO_SPEC_V1_2, "2.7.5");
