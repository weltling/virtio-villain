/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0031: balloon INFLATE with the PFN array straddling a page boundary.
 *
 * Spec 5.5.6: The inflate queue carries an array of 32 bit PFNs.
 * Submit a single INFLATE descriptor whose buffer starts 64 bytes
 * before a page boundary and extends 256 bytes past it, so the
 * PFN array spans two physically adjacent pages mapped through
 * one virtual buffer. The device must walk the buffer linearly
 * without assuming page alignment.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_balloon_inflate_cross_page(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    uint8_t *region = vv_alloc_pages(2);
    /* Place the PFN array 64 bytes before the page boundary. */
    uint32_t *pfns = (uint32_t *)(region + 4096 - 64);
    /* 80 PFNs total => 320 bytes => crosses the boundary. */
    for (int i = 0; i < 80; i++)
        pfns[i] = (uint32_t)(0x100 + i);

    uint64_t phys = vv_virt_to_phys(pfns);
    uint32_t len  = (uint32_t)(80 * sizeof(uint32_t));

    vring_raw_set_desc(vr, 0, phys, len, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(L0031, VIRTIO_PCI_DEVICE_BALLOON,
              test_balloon_inflate_cross_page,
              "INFLATE PFN array straddling a page boundary",
              VIRTIO_SPEC_V1_2, "5.5.6");
