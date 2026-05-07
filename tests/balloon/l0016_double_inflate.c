/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0016: Balloon inflate then re-inflate same PFN.
 *
 * Inflate a PFN, wait for completion, then inflate the same PFN
 * again without an intervening deflate. The device must not
 * crash from double-tracking.
 *
 * Spec 5.5.6.1.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

#define VIRTIO_BALLOON_PFN_SHIFT 12

static test_result_t test_balloon_double_inflate(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    uint32_t *pfns = vv_alloc_pages(1);
    uint64_t base = vv_virt_to_phys(vv_alloc_pages(1));
    pfns[0] = (uint32_t)(base >> VIRTIO_BALLOON_PFN_SHIFT);
    uint64_t p = vv_virt_to_phys(pfns);

    vring_raw_set_desc(vr, 0, p, 4, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    vring_raw_set_desc(vr, 1, p, 4, 0, 0);
    vring_raw_set_avail(vr, 1, 1);
    vring_raw_set_avail_idx(vr, 2);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(L0016, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_double_inflate,
              "Re-inflate same PFN",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
