/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0002: Balloon deflate with valid PFNs.
 *
 * Submit PFNs to the deflate queue (queue 1) to tell the device
 * the guest is reclaiming memory.
 *
 * Spec 5.5.6.1: The driver places PFNs on the deflateq to
 * indicate pages that should be returned to the guest.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_balloon_deflate(struct virtio_dev *dev,
                                          struct vring *vr)
{
    /* Use deflate queue (queue 1) */
    struct vring deflate_vr;
    vring_alloc(&deflate_vr, 64);
    vring_attach(dev, &deflate_vr, 1);

    uint32_t *pfns = vv_alloc_pages(1);
    uint64_t base_addr = vv_virt_to_phys(vv_alloc_pages(4));
    for (int i = 0; i < 4; i++)
        pfns[i] = (uint32_t)((base_addr >> VIRTIO_BALLOON_PFN_SHIFT) + i);

    uint64_t pfns_phys = vv_virt_to_phys(pfns);

    vring_raw_set_desc(&deflate_vr, 0, pfns_phys, 4 * sizeof(uint32_t), 0, 0);
    vring_raw_set_avail(&deflate_vr, 0, 0);
    vring_raw_set_avail_idx(&deflate_vr, 1);

    (void)vr;
    return vv_kick_and_wait(dev, &deflate_vr, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST(L0002, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_deflate,
              "Deflate with valid page frame numbers",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
