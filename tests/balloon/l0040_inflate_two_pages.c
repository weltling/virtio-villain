/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0040: inflate two pages in one descriptor.
 *
 * Submit two consecutive PFN entries (8 bytes) in a single
 * descriptor to the inflate queue. The device must consume both
 * PFNs from the same buffer. Tests batch inflate.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_balloon_inflate_two(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint32_t *pfns = vv_alloc_pages(1);
    void *p1 = vv_alloc_pages(1);
    void *p2 = vv_alloc_pages(1);

    pfns[0] = (uint32_t)(vv_virt_to_phys(p1) >> VIRTIO_BALLOON_PFN_SHIFT);
    pfns[1] = (uint32_t)(vv_virt_to_phys(p2) >> VIRTIO_BALLOON_PFN_SHIFT);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(pfns), 8, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(L0040, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_inflate_two,
              "Inflate two pages in one descriptor",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
