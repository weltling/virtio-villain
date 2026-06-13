/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0017: Balloon deflate without prior inflate.
 *
 * Submit PFNs to deflateq for pages that were never inflated.
 * The device must not return memory it does not own; it must
 * either ignore or treat as a no-op.
 *
 * Spec 5.5.6.1.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_balloon_deflate_unbacked(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct vring deflate_vr;
    vring_alloc(&deflate_vr, 64);
    vring_attach(dev, &deflate_vr, 1);

    uint32_t *pfns = vv_alloc_pages(1);
    uint64_t base = vv_virt_to_phys(vv_alloc_pages(1));
    pfns[0] = (uint32_t)(base >> VIRTIO_BALLOON_PFN_SHIFT);
    uint64_t p = vv_virt_to_phys(pfns);

    vring_raw_set_desc(&deflate_vr, 0, p, 4, 0, 0);
    vring_raw_set_avail(&deflate_vr, 0, 0);
    vring_raw_set_avail_idx(&deflate_vr, 1);

    (void)vr;
    return vv_kick_and_wait(dev, &deflate_vr, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST(L0017, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_deflate_unbacked,
              "Deflate never-inflated PFN",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
