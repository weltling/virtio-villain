/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0006: Balloon inflate then deflate the same PFNs.
 *
 * Inflate a set of PFNs, then deflate them back. Exercises the
 * full balloon lifecycle.
 *
 * Spec 5.5.6.1: Full inflate/deflate cycle.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_BALLOON_PFN_SHIFT 12

static test_result_t test_balloon_inflate_deflate(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    /* Set up deflate queue */
    struct vring deflate_vr;
    vring_alloc(&deflate_vr, 64);
    vring_attach(dev, &deflate_vr, 1);

    uint32_t *pfns = vv_alloc_pages(1);
    uint64_t base = vv_virt_to_phys(vv_alloc_pages(4));
    for (int i = 0; i < 4; i++)
        pfns[i] = (uint32_t)((base >> VIRTIO_BALLOON_PFN_SHIFT) + i);

    uint64_t pfns_phys = vv_virt_to_phys(pfns);

    /* Inflate */
    vring_raw_set_desc(vr, 0, pfns_phys, 4 * sizeof(uint32_t), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    /* Deflate same PFNs */
    vring_raw_set_desc(&deflate_vr, 0, pfns_phys, 4 * sizeof(uint32_t), 0, 0);
    vring_raw_set_avail(&deflate_vr, 0, 0);
    vring_raw_set_avail_idx(&deflate_vr, 1);

    return vv_kick_and_wait(dev, &deflate_vr, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST(L0006, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_inflate_deflate,
              "Inflate then deflate same pages",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
