/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0004: writable descriptor whose length runs past end of RAM.
 *
 * The virtio spec lets the device write up to len bytes into a
 * writable descriptor, so claiming a huge len with a small backing
 * allocation is a driver bug, not a VMM bug (the device will simply
 * clobber the driver's own memory).
 *
 * The interesting boundary is the end of guest physical memory. If
 * the device honors a len that puts addr + len past the last byte of
 * guest RAM, it must either truncate the write to the in-range part
 * or refuse the descriptor entirely. It must not access memory
 * outside the guest's mapping or crash the VMM.
 *
 * This test uses vv_parse_ram_top() to locate the top of System RAM,
 * allocates a page whose base sits as close as possible below that
 * top, and programs a writable descriptor whose base is that page but
 * whose length crosses far past the end of guest RAM. Placing the base
 * near the top keeps the in-range prefix a "write then fault" device
 * fills small and away from the init image. Pass means the device
 * completed the
 * request, silently rejected it, or wedged the queue. Any of those
 * is acceptable. Triple faulting the guest or crashing the VMM is
 * not.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static test_result_t test_rng_huge_len(struct virtio_dev *dev,
                                       struct vring *vr)
{
    uint64_t ram_top = vv_parse_ram_top();
    if (ram_top == 0)
        return TEST_SKIP;

    uint64_t buf_phys;
    uint8_t *buf = vv_alloc_page_near_ram_top(ram_top, &buf_phys);
    if (!buf)
        return TEST_SKIP;

    /*
     * Pick a length that overshoots the end of guest RAM by 1 GiB.
     * The base is a valid in-range GPA so the device cannot bail on
     * the base alone; it has to validate addr + len.
     */
    uint64_t overshoot = (ram_top - buf_phys) + (1ULL << 30);
    if (overshoot > 0xFFFFFFFFULL)
        overshoot = 0xFFFFFFFFULL;
    uint32_t len = (uint32_t)overshoot;

    vring_raw_set_desc(vr, 0, buf_phys, len, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RNG0004, VIRTIO_PCI_DEVICE_RNG, test_rng_huge_len,
              "RNG writable descriptor length crosses end of RAM",
              VIRTIO_SPEC_V1_2, "2.7.5");
