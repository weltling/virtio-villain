/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0004: Balloon inflate with invalid (high) PFNs.
 *
 * Submit PFNs that point beyond physical memory. The device
 * should reject or ignore them without crashing.
 *
 * Spec 5.5.6.1: Robustness test.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_balloon_inflate_bad_pfn(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    uint32_t *pfns = vv_alloc_pages(1);

    /* PFNs pointing to absurdly high physical addresses */
    pfns[0] = 0xFFFFFFFF;
    pfns[1] = 0xFFFFFFFE;
    pfns[2] = 0xDEADBEEF;
    pfns[3] = 0x00000000;

    uint64_t pfns_phys = vv_virt_to_phys(pfns);

    vring_raw_set_desc(vr, 0, pfns_phys, 4 * sizeof(uint32_t), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(L0004, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_inflate_bad_pfn,
              "Inflate with invalid PFNs beyond physical memory",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
